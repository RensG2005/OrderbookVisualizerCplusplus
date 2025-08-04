cmake_minimum_required(VERSION 3.15)
project(websocket_client_tls)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

find_package(nlohmann_json CONFIG REQUIRED)

add_executable(main main.cpp)

target_link_libraries(main PRIVATE
    Boost::system 
    Boost::thread 
    OpenSSL::SSL 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(main PRIVATE ws2_32 wsock32)
endif()