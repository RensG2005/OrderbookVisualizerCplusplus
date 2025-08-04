#include "OrderBook.hpp"
#include <algorithm>



OrderBookLevel::OrderBookLevel(double p, double q, const std::string& ts)
    : price(p), quantity(q), timestamp(ts) {}

CoinbaseOrderBook::CoinbaseOrderBook(const std::string& sym)
    : symbol(sym), last_update(std::chrono::system_clock::now()) {}

void CoinbaseOrderBook::updateLevel(const std::string& side, double price, double quantity, const std::string& timestamp) {
    std::lock_guard<std::mutex> lock(book_mutex);
    auto& book = (side == "bid") ? bids : asks;
    if (quantity < 0.05) {
        book.erase(price);
    } else {
        book[price] = OrderBookLevel(price, quantity, timestamp);
    }
    last_update = std::chrono::system_clock::now();
}
    
std::vector<OrderBookLevel> CoinbaseOrderBook::getBids(int depth) const {
    std::lock_guard<std::mutex> lock(book_mutex);
    std::vector<OrderBookLevel> result;
    
    auto it = bids.rbegin();
    for (int i = 0; i < depth && it != bids.rend(); ++i, ++it) {
        result.push_back(it->second);
    }
    return result;
}
    
std::vector<OrderBookLevel> CoinbaseOrderBook::getAsks(int depth) const {
    std::lock_guard<std::mutex> lock(book_mutex);
    std::vector<OrderBookLevel> result;
    auto it = asks.begin();
    for (int i = 0; i < depth && it != asks.end(); ++i, ++it) {
        result.push_back(it->second);
    }
    return result;
}
    
double CoinbaseOrderBook::getBestBid() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return bids.empty() ? 0.0 : bids.rbegin()->first;
}
    
double CoinbaseOrderBook::getBestAsk() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return asks.empty() ? 0.0 : asks.begin()->first;
}
    
double CoinbaseOrderBook::getSpread() const {
    double bid = getBestBid();
    double ask = getBestAsk();
    return (bid > 0 && ask > 0) ? ask - bid : 0.0;
}
    
double CoinbaseOrderBook::getSpreadBps() const {
    double bid = getBestBid();
    double ask = getBestAsk();
    if (bid > 0 && ask > 0) {
        return ((ask - bid) / ((ask + bid) / 2.0)) * 10000.0;
    }
    return 0.0;
}
    
std::chrono::system_clock::time_point CoinbaseOrderBook::getLastUpdate() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return last_update;
}
    
size_t CoinbaseOrderBook::getBidLevels() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return bids.size();
}
    
size_t CoinbaseOrderBook::getAskLevels() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return asks.size();
}