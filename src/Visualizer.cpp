#include "Visualizer.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

OrderBookVisualizer::OrderBookVisualizer(CoinbaseOrderBook& ob, const std::string& sym)
    : book(ob), symbol(sym) {
    getTerminalSize();
}

void OrderBookVisualizer::clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void OrderBookVisualizer::getTerminalSize() {
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

std::string OrderBookVisualizer::createBar(double value, double max_value, int max_width, char fill_char) {
    if (max_value <= 0) return "";
    int bar_length = static_cast<int>((value / max_value) * max_width);
    return std::string(bar_length, fill_char);
}

void OrderBookVisualizer::printHeader() {
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

void OrderBookVisualizer::visualize() {
    clearScreen();
    getTerminalSize();

    printHeader();

    std::vector<OrderBookLevel> bids = book.getBids(10);
    std::vector<OrderBookLevel> asks = book.getAsks(10);

    double max_bid_qty = 0.0, max_ask_qty = 0.0;
    for (const auto& bid : bids) max_bid_qty = std::max(max_bid_qty, bid.quantity);
    for (const auto& ask : asks) max_ask_qty = std::max(max_ask_qty, ask.quantity);

    double max_qty = std::max(max_bid_qty, max_ask_qty);
    int bar_width = std::min(40, terminal_width / 3);

    std::cout << "\033[31m"; // Red for asks
    std::cout << "ASKS (Sellers) - " << asks.size() << " levels\n";
    std::cout << "════════════════════════════════════════════════════════════════════════════\n";
    std::cout << std::setw(12) << "Price ($)" << " │ " << std::setw(15) << "Quantity" << " │ " << "Liquidity\n";
    std::cout << "─────────────┼─────────────────┼────────────────────────────────────────────\n";
    for (int i = asks.size() - 1; i >= 0; --i) {
        const auto& ask = asks[i];
        std::string bar = createBar(ask.quantity, max_qty, bar_width, 'X');
        std::cout << "$" << std::fixed << std::setprecision(3) << std::setw(10) << ask.price
                  << " │ " << std::setw(15) << std::setprecision(2) << ask.quantity
                  << " │ " << bar << "\n";
    }

    std::cout << "\033[0m";
    std::cout << "                          ── SPREAD: $" << std::fixed << std::setprecision(2)
              << book.getSpread() << " (" << std::setprecision(1) << book.getSpreadBps() << " bps) ──\n";

    std::cout << "\033[32m"; // Green for bids
    for (const auto& bid : bids) {
        std::string bar = createBar(bid.quantity, max_qty, bar_width, 'X');
        std::cout << "$" << std::fixed << std::setprecision(3) << std::setw(10) << bid.price
                  << " │ " << std::setw(15) << std::setprecision(2) << bid.quantity
                  << " │ " << bar << "\n";
    }

    double total_bid_volume = 0.0, total_bid_value = 0.0;
    double total_ask_volume = 0.0, total_ask_value = 0.0;

    for (const auto& ask : asks) {
        total_ask_volume += ask.quantity;
        total_ask_value += ask.quantity * ask.price;
    }
    for (const auto& bid : bids) {
        total_bid_volume += bid.quantity;
        total_bid_value += bid.quantity * bid.price;
    }

    std::cout << "\033[0m\n";
    std::cout << "Ask Volume (top 10): " << std::setprecision(2) << total_ask_volume << " " << symbol.substr(0, 3) << "\n";
    std::cout << "Ask Value (top 10): $" << std::setprecision(2) << total_ask_value << "\n";
    std::cout << "Bid Volume (top 10): " << std::fixed << std::setprecision(2) << total_bid_volume << " " << symbol.substr(0, 3) << "\n";
    std::cout << "Bid Value (top 10): $" << std::setprecision(2) << total_bid_value << "\n";

    std::cout << "\nPress Ctrl+C to exit...\n";
}
