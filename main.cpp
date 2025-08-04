#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
    #include <termios.h>
#endif

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

struct OrderBookLevel {
    double price;
    double quantity;
    std::string timestamp;
    
    OrderBookLevel(double p = 0.0, double q = 0.0, const std::string& ts = "") 
        : price(p), quantity(q), timestamp(ts) {}
};

class CoinbaseOrderBook {
private:
    std::map<double, OrderBookLevel> bids;
    std::map<double, OrderBookLevel> asks;
    mutable std::mutex book_mutex;
    std::string symbol;
    std::chrono::system_clock::time_point last_update;
    
public:
    CoinbaseOrderBook(const std::string& sym) : symbol(sym) {
        last_update = std::chrono::system_clock::now();
    }
    
    void updateLevel(const std::string& side, double price, double quantity, const std::string& timestamp) {
        std::lock_guard<std::mutex> lock(book_mutex);
        
        if (side == "bid") {
            if (quantity == 0.0) {
                bids.erase(price);
            } else {
                bids[price] = OrderBookLevel(price, quantity, timestamp);
            }
        } else if (side == "offer") {
            if (quantity == 0.0) {
                asks.erase(price);
            } else {
                asks[price] = OrderBookLevel(price, quantity, timestamp);
            }
        }
        
        last_update = std::chrono::system_clock::now();
    }
    
    std::vector<OrderBookLevel> getBids(int depth = 20) const {
        std::lock_guard<std::mutex> lock(book_mutex);
        std::vector<OrderBookLevel> result;
        
        auto it = bids.rbegin(); // Start from highest price
        for (int i = 0; i < depth && it != bids.rend(); ++i, ++it) {
            result.push_back(it->second);
        }
        return result;
    }
    
    std::vector<OrderBookLevel> getAsks(int depth = 20) const {
        std::lock_guard<std::mutex> lock(book_mutex);
        std::vector<OrderBookLevel> result;
        auto it = asks.begin();
        for (int i = 0; i < depth && it != asks.end(); ++i, ++it) {
            result.push_back(it->second);
        }
        return result;
    }
    
    double getBestBid() const {
        std::lock_guard<std::mutex> lock(book_mutex);
        return bids.empty() ? 0.0 : bids.rbegin()->first;
    }
    
    double getBestAsk() const {
        std::lock_guard<std::mutex> lock(book_mutex);
        return asks.empty() ? 0.0 : asks.begin()->first;
    }
    
    double getSpread() const {
        double bid = getBestBid();
        double ask = getBestAsk();
        return (bid > 0 && ask > 0) ? ask - bid : 0.0;
    }
    
    double getSpreadBps() const {
        double bid = getBestBid();
        double ask = getBestAsk();
        if (bid > 0 && ask > 0) {
            return ((ask - bid) / ((ask + bid) / 2.0)) * 10000.0; // basis points
        }
        return 0.0;
    }
    
    std::chrono::system_clock::time_point getLastUpdate() const {
        std::lock_guard<std::mutex> lock(book_mutex);
        return last_update;
    }
    
    size_t getBidLevels() const {
        std::lock_guard<std::mutex> lock(book_mutex);
        return bids.size();
    }
    
    size_t getAskLevels() const {
        std::lock_guard<std::mutex> lock(book_mutex);
        return asks.size();
    }
};

class OrderBookVisualizer {
private:
    CoinbaseOrderBook& book;
    std::string symbol;
    int terminal_width;
    int terminal_height;
    
    void clearScreen() {
        #ifdef _WIN32
                system("cls");
        #else
                system("clear");
        #endif
    }
    
    void getTerminalSize() {
        #ifdef _WIN32
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
                terminal_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                terminal_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        #else
                struct winsize w;
                ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                terminal_width = w.ws_col;
                terminal_height = w.ws_row;
        #endif
    }
    
    std::string createBar(double value, double max_value, int max_width, char fill_char = 'X') {
        if (max_value <= 0) return "";
        
        int bar_length = static_cast<int>((value / max_value) * max_width);
        return std::string(bar_length, fill_char);
    }
    
    void printHeader() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "╔═══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                      COINBASE PRO ORDER BOOK VISUALIZER                       ║\n";
        std::cout << "╠═══════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║ Symbol: " << std::setw(10) << std::left << symbol 
                  << "Time: " << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << std::string(30, ' ') << "║\n";
        
        double spread = book.getSpread();
        double spread_bps = book.getSpreadBps();
        double best_bid = book.getBestBid();
        double best_ask = book.getBestAsk();
        
        std::cout << "║ Best Bid: $" << std::fixed << std::setprecision(2) << std::setw(8) << best_bid 
                  << " | Best Ask: $" << std::setw(8) << best_ask 
                  << " | Spread: $" << std::setw(6) << spread << " (" << std::setprecision(1) << spread_bps << " bps)" 
                  << std::string(5, ' ') << "║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════════════════════╝\n\n";
    }
    
public:
    OrderBookVisualizer(CoinbaseOrderBook& ob, const std::string& sym) : book(ob), symbol(sym) {
        getTerminalSize();
    }
    
    void visualize() {
        clearScreen();
        getTerminalSize();
        
        printHeader();
        
        std::vector<OrderBookLevel> bids = book.getBids(15);
        std::vector<OrderBookLevel> asks = book.getAsks(15);

        double max_bid_qty = 0.0;
        double max_ask_qty = 0.0;
        
        for (const auto& bid : bids) {
            max_bid_qty = std::max(max_bid_qty, bid.quantity);
        }
        for (const auto& ask : asks) {
            max_ask_qty = std::max(max_ask_qty, ask.quantity);
        }
        
        double max_qty = std::max(max_bid_qty, max_ask_qty);
        int bar_width = std::min(40, terminal_width / 3);
        
        std::cout << "\033[31m"; // Red color for asks
        std::cout << "ASKS (Sellers) - " << asks.size() << " levels\n";
        std::cout << "════════════════════════════════════════════════════════════════════════════\n";
        std::cout << std::setw(12) << "Price ($)" << " │ " << std::setw(15) << "Quantity" << " │ " << "Liquidity\n";
        std::cout << "─────────────┼─────────────────┼────────────────────────────────────────────\n";
        
        for (int i = asks.size() - 1; i >= 0; --i) {
            const auto& ask = asks[i];
            std::string bar = createBar(ask.quantity, max_qty, bar_width, 'X');
            
            std::cout << "$" << std::fixed << std::setprecision(2) << std::setw(10) << ask.price 
                      << " │ " << std::setw(15) << std::setprecision(8) << ask.quantity 
                      << " │ " << bar << "\n";
        }
        
        std::cout << "\033[0m"; // Reset color
        
        std::cout << "\n";
        std::cout << "                          ── SPREAD: $" << std::fixed << std::setprecision(2) 
                  << book.getSpread() << " (" << std::setprecision(1) << book.getSpreadBps() << " bps) ──\n\n";
        
        std::cout << "\033[32m"; // Green color for bids
        std::cout << "BIDS (Buyers) - " << bids.size() << " levels\n";
        std::cout << "════════════════════════════════════════════════════════════════════════════\n";
        std::cout << std::setw(12) << "Price ($)" << " │ " << std::setw(15) << "Quantity" << " │ " << "Liquidity\n";
        std::cout << "─────────────┼─────────────────┼────────────────────────────────────────────\n";
        
        for (const auto& bid : bids) {
            std::string bar = createBar(bid.quantity, max_qty, bar_width, 'X');
            
            std::cout << "$" << std::fixed << std::setprecision(2) << std::setw(10) << bid.price 
                      << " │ " << std::setw(15) << std::setprecision(8) << bid.quantity 
                      << " │ " << bar << "\n";
        }
        
        std::cout << "\033[0m"; // Reset color
        
        std::cout << "\n";
        std::cout << "Market Statistics:\n";
        
        double total_bid_volume = 0.0;
        double total_ask_volume = 0.0;
        double total_bid_value = 0.0;
        double total_ask_value = 0.0;
        
        for (const auto& bid : bids) {
            total_bid_volume += bid.quantity;
            total_bid_value += bid.quantity * bid.price;
        }
        for (const auto& ask : asks) {
            total_ask_volume += ask.quantity;
            total_ask_value += ask.quantity * ask.price;
        }
        
        std::cout << "Bid Volume (top 15): " << std::fixed << std::setprecision(4) << total_bid_volume << " " << symbol.substr(0, 3) << "\n";
        std::cout << "Ask Volume (top 15): " << std::setprecision(4) << total_ask_volume << " " << symbol.substr(0, 3) << "\n";
        std::cout << "Bid Value (top 15): $" << std::setprecision(2) << total_bid_value << "\n";
        std::cout << "Ask Value (top 15): $" << std::setprecision(2) << total_ask_value << "\n";
        
        std::cout << "\nPress Ctrl+C to exit...\n";
    }
};

class CoinbaseWebSocketClient {
private:
    std::string host;
    std::string port;
    std::string symbol;
    CoinbaseOrderBook& order_book;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws;
    net::io_context ioc;
    ssl::context ctx;
    std::thread ws_thread;
    bool running;
    
public:
    CoinbaseWebSocketClient(const std::string& trading_pair, CoinbaseOrderBook& ob) 
        : host("advanced-trade-ws.coinbase.com"), port("443"), symbol(trading_pair), 
          order_book(ob), ctx(ssl::context::tlsv12_client), running(false) {
        
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);
    }
    
    void handleMessage(const std::string& message) {
        try {
            json j = json::parse(message);
            
            if (j.contains("channel") && j["channel"] == "l2_data") {
                if (j.contains("events")) {
                    for (const auto& event : j["events"]) {
                        if (event.contains("type") && event["type"] == "update") {
                            if (event.contains("updates")) {
                                for (const auto& update : event["updates"]) {
                                    std::string side = update["side"];
                                    double price = std::stod(update["price_level"].get<std::string>());
                                    double quantity = std::stod(update["new_quantity"].get<std::string>());
                                    std::string timestamp = update.value("event_time", "");
                                    
                                    order_book.updateLevel(side, price, quantity, timestamp);
                                }
                            }
                        } else if (event.contains("type") && event["type"] == "snapshot") {
                            if (event.contains("updates")) {
                                for (const auto& update : event["updates"]) {
                                    std::string side = update["side"];
                                    double price = std::stod(update["price_level"].get<std::string>());
                                    double quantity = std::stod(update["new_quantity"].get<std::string>());
                                    std::string timestamp = update.value("event_time", "");
                                    
                                    order_book.updateLevel(side, price, quantity, timestamp);
                                }
                            }
                        }
                    }
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing message: " << e.what() << std::endl;
            std::cerr << "Message: " << message.substr(0, 200) << "..." << std::endl;
        }
    }
    
    void connect() {
        try {
            running = true;
            ws_thread = std::thread([this]() {
                try {
                    ws = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(ioc, ctx);
                    
                    tcp::resolver resolver(ioc);
                    auto const results = resolver.resolve(host, port);
                    
                    auto ep = net::connect(ws->next_layer().next_layer(), results);
                    
                    if(!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.c_str())) {
                        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                        throw beast::system_error{ec};
                    }
                    
                    ws->next_layer().handshake(ssl::stream_base::client);
                    
                    ws->set_option(websocket::stream_base::decorator(
                        [](websocket::request_type& req) {
                            req.set(http::field::user_agent, "CoinbaseOrderBookViz/1.0");
                        }));
                    
                    ws->handshake(host, "/");
                    
                    std::string subscription = "{\"type\":\"subscribe\",\"channel\":\"level2\",\"product_ids\":[\"" + symbol + "\"]}";
                    ws->write(net::buffer(subscription));
                    
                    std::cout << "\033[2J";
                    std::cout << "Connected to Coinbase WebSocket for " << symbol << std::endl;
                    
                    while (running) {
                        beast::flat_buffer buffer;
                        ws->read(buffer);
                        
                        std::string message = beast::buffers_to_string(buffer.data());
                        handleMessage(message);
                        
                        buffer.clear();
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "WebSocket error: " << e.what() << std::endl;
                    running = false;
                }
            });
            
        } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
        }
    }
    
    void disconnect() {
        running = false;
        if (ws) {
            try {
                ws->close(websocket::close_code::normal);
            } catch (...) {
            }
        }
        if (ws_thread.joinable()) {
            ws_thread.join();
        }
    }
    
    ~CoinbaseWebSocketClient() {
        disconnect();
    }
};

int main(int argc, char** argv) {
    try {
        if(argc != 2) {
            std::cerr << "Usage: ./coinbase-orderbook <TRADING_PAIR>\n";
            std::cerr << "Example:\n";
            std::cerr << "  ./coinbase-orderbook BTC-USD\n";
            return EXIT_FAILURE;
        }
        
        std::string trading_pair = argv[1];
        
        std::cout << "Connecting to Coinbase Advanced Trade WebSocket..." << std::endl;
        
        CoinbaseOrderBook order_book(trading_pair);
        OrderBookVisualizer visualizer(order_book, trading_pair);
        CoinbaseWebSocketClient ws_client(trading_pair, order_book);
        
        ws_client.connect();
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        auto last_update = std::chrono::steady_clock::now();
        const auto update_interval = std::chrono::milliseconds(1000);
        
        while (true) {
            auto now = std::chrono::steady_clock::now();
            
            if (now - last_update >= update_interval) {
                visualizer.visualize();
                last_update = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}