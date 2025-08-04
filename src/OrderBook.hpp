#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>

struct OrderBookLevel {
    double price;
    double quantity;
    std::string timestamp;

    OrderBookLevel(double p = 0.0, double q = 0.0, const std::string& ts = "");
};

class CoinbaseOrderBook {
public:
    CoinbaseOrderBook(const std::string& sym);
    void updateLevel(const std::string& side, double price, double quantity, const std::string& timestamp);
    std::vector<OrderBookLevel> getBids(int depth = 20) const;
    std::vector<OrderBookLevel> getAsks(int depth = 20) const;
    double getBestBid() const;
    double getBestAsk() const;
    double getSpread() const;
    double getSpreadBps() const;
    std::chrono::system_clock::time_point getLastUpdate() const;
    size_t getBidLevels() const;
    size_t getAskLevels() const;

private:
    std::map<double, OrderBookLevel> bids;
    std::map<double, OrderBookLevel> asks;
    mutable std::mutex book_mutex;
    std::string symbol;
    std::chrono::system_clock::time_point last_update;
};
