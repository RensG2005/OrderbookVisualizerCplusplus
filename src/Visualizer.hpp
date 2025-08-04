#pragma once
#include "OrderBook.hpp"
#include <string>

class OrderBookVisualizer {
public:
    OrderBookVisualizer(CoinbaseOrderBook& ob, const std::string& sym);
    void visualize();

private:
    CoinbaseOrderBook& book;
    std::string symbol;
    int terminal_width, terminal_height;

    void clearScreen();
    void getTerminalSize();
    void printHeader();
    std::string createBar(double value, double max_value, int max_width, char fill_char = 'X');
};