cmake_minimum_required(VERSION 3.15)
project(network_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 查找依赖 - 使用 Protobuf 3.19.5
# ============================================================
set(Protobuf_INCLUDE_DIR "/opt/protobuf-3.19.5/include")
set(Protobuf_LIBRARIES "/opt/protobuf-3.19.5/lib/libprotobuf.a")

find_package(Boost REQUIRED COMPONENTS system)

# ============================================================
# 测试程序 1: network_test_app (从 app.cpp)
# ============================================================
add_executable(network_test_app app.cpp)

target_include_directories(network_test_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto_v19
    ${Protobuf_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(network_test_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libnetwork.dylib
    ${Protobuf_LIBRARIES}
    ${Boost_LIBRARIES}
    pthread
)

# ============================================================
# 测试程序 2: minimal_test (从 minimal_test.cpp)
# ============================================================
add_executable(minimal_test minimal_test.cpp)

target_include_directories(minimal_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto_v19
    ${Protobuf_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(minimal_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libnetwork.dylib
    ${Protobuf_LIBRARIES}
    ${Boost_LIBRARIES}
    pthread
)

# Windows 需要链接 winsock
if(WIN32)
    target_link_libraries(network_test_app PRIVATE ws2_32)
    target_link_libraries(minimal_test PRIVATE ws2_32)
endif()

message(STATUS "Test apps configured successfully")
