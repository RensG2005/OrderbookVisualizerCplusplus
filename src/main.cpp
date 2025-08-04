#include <iostream>
#include <thread>
#include <chrono>
#include "OrderBook.hpp"
#include "Visualizer.hpp"
#include "WebSocketClient.hpp"

int main(int argc, char** argv) {
    try {
        if(argc != 2) {
            std::cerr << "Usage: ./coinbase-orderbook <TRADING_PAIR>\n";
            return EXIT_FAILURE;
        }
        
        std::string trading_pair = argv[1];
        
        std::cout << "Connecting to Coinbase Advanced Trade WebSocket..." << std::endl;
        
        CoinbaseOrderBook order_book(trading_pair);
        OrderBookVisualizer visualizer(order_book, trading_pair);
        CoinbaseWebSocketClient ws_client(trading_pair, order_book);
        
        ws_client.connect();
        
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