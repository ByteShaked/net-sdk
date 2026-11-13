cmake_minimum_required(VERSION 3.15)
project(network_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 查找依赖
# ============================================================
find_package(Protobuf REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS system)

# ============================================================
# 测试程序（不编译 proto 源码，libnetwork.dylib 里已包含）
# ============================================================
add_executable(network_test_app app.cpp)

target_include_directories(network_test_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto_v19
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(network_test_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libnetwork.dylib
    protobuf::libprotobuf
    ${Boost_LIBRARIES}
    pthread
)

# Windows 需要链接 winsock
if(WIN32)
    target_link_libraries(network_test_app PRIVATE ws2_32)
endif()

message(STATUS "Test app configured successfully")
