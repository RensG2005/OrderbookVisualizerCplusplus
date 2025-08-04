#include "WebSocketClient.hpp"
#include <boost/asio/connect.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

CoinbaseWebSocketClient::CoinbaseWebSocketClient(const std::string& trading_pair, CoinbaseOrderBook& ob)
    : host("advanced-trade-ws.coinbase.com"),
      port("443"),
      symbol(trading_pair),
      order_book(ob),
      ctx(ssl::context::tlsv12_client),
      running(false) {

    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);
}

void CoinbaseWebSocketClient::handleMessage(const std::string& message) {
    try {
        json j = json::parse(message);

        if (j.contains("channel") && j["channel"] == "l2_data") {
            if (j.contains("events")) {
                for (const auto& event : j["events"]) {
                    if (event.contains("type") && event["type"] == "update") {
                        for (const auto& update : event["updates"]) {
                            std::string side = update["side"];
                            double price = std::stod(update["price_level"].get<std::string>());
                            double quantity = std::stod(update["new_quantity"].get<std::string>());
                            std::string timestamp = update.value("event_time", "");
                            order_book.updateLevel(side, price, quantity, timestamp);
                        }
                    } else if (event.contains("type") && event["type"] == "snapshot") {
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

    } catch (const std::exception& e) {
        std::cerr << "Error parsing message: " << e.what() << "\n"
                  << "Message: " << message.substr(0, 200) << "...\n";
    }
}

void CoinbaseWebSocketClient::connect() {
    try {
        running = true;
        ws_thread = std::thread([this]() {
            try {
                ws = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(ioc, ctx);
                tcp::resolver resolver(ioc);
                auto const results = resolver.resolve(host, port);
                net::connect(ws->next_layer().next_layer(), results);

                if (!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.c_str())) {
                    beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                    throw beast::system_error{ec};
                }

                ws->next_layer().handshake(ssl::stream_base::client);

                ws->set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
                    req.set(http::field::user_agent, "CoinbaseOrderBookViz/1.0");
                }));

                ws->handshake(host, "/");

                std::string subscription = R"({"type":"subscribe","channel":"level2","product_ids":[")" + symbol + R"("]})";
                ws->write(net::buffer(subscription));

                std::cout << "\033[2J";  // Clear terminal
                std::cout << "Connected to Coinbase WebSocket for " << symbol << std::endl;

                while (running) {
                    beast::flat_buffer buffer;
                    ws->read(buffer);
                    std::string message = beast::buffers_to_string(buffer.data());
                    handleMessage(message);
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

void CoinbaseWebSocketClient::disconnect() {
    running = false;
    if (ws) {
        try {
            ws->close(websocket::close_code::normal);
        } catch (...) {}
    }
    if (ws_thread.joinable()) {
        ws_thread.join();
    }
}

CoinbaseWebSocketClient::~CoinbaseWebSocketClient() {
    disconnect();
}
