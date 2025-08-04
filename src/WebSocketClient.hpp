#pragma once
#include "OrderBook.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <memory>

class CoinbaseWebSocketClient {
public:
    CoinbaseWebSocketClient(const std::string& trading_pair, CoinbaseOrderBook& ob);
    void connect();
    void disconnect();
    ~CoinbaseWebSocketClient();

private:
    void handleMessage(const std::string& message);

    std::string host, port, symbol;
    CoinbaseOrderBook& order_book;
    std::unique_ptr<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>> ws;
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx;
    std::thread ws_thread;
    bool running;
};