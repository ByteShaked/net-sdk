cmake_minimum_required(VERSION 3.15)
project(im_network VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# 平台检测
# ============================================================
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

# ============================================================
# 版本号
# ============================================================
if(NOT BUILD_VERSION)
    set(BUILD_VERSION 0)
endif()
if(NOT MAJOR_VERSION)
    set(MAJOR_VERSION 1)
endif()
if(NOT MINOR_VERSION)
    set(MINOR_VERSION 0)
endif()
if(NOT RELEASE_VERSION)
    set(RELEASE_VERSION 0)
endif()
add_definitions(
    -DBUILD_VERSION=${BUILD_VERSION}
    -DMAJOR_VERSION=${MAJOR_VERSION}
    -DMINOR_VERSION=${MINOR_VERSION}
    -DRELEASE_VERSION=${RELEASE_VERSION}
)

# ============================================================
# 查找依赖（自动适配各平台安装路径）
# ============================================================
# 使用自编译的 Protobuf 3.19.5，完全匹配 proto_v19 生成代码的版本
set(CMAKE_PREFIX_PATH "/opt/protobuf-3.19.5" ${CMAKE_PREFIX_PATH})
set(Protobuf_INCLUDE_DIR "/opt/protobuf-3.19.5/include")
set(Protobuf_LIBRARIES "/opt/protobuf-3.19.5/lib/libprotobuf.a")
set(Protobuf_PROTOC_EXECUTABLE "/opt/protobuf-3.19.5/bin/protoc")

find_package(Boost REQUIRED COMPONENTS system)
find_package(ZLIB REQUIRED)             # protobuf 3.19.x 依赖 zlib

message(STATUS "Protobuf version:  ${Protobuf_VERSION}")
message(STATUS "Protobuf include:  ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libs:     ${Protobuf_LIBRARIES}")
message(STATUS "Boost include:     ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost libs:        ${Boost_LIBRARIES}")

# ============================================================
# proto 生成文件（从 proto_v19/ 复用，不重新生成）
# ============================================================
file(GLOB_RECURSE PROTO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/proto_v19/*.pb.cc"
)

# ============================================================
# 源文件
# ============================================================
set(NETWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NetworkModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TlvHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NetLog.cpp
    ${PROTO_SOURCES}
)

# ============================================================
# 头文件路径
# ============================================================
set(NETWORK_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/proto_v19          # 3.19.5 头文件，必须在系统 protobuf 前面
    ${Boost_INCLUDE_DIRS}
)

# ============================================================
# 编译动态库 libnetwork
# ============================================================
add_library(network SHARED ${NETWORK_SOURCES})

target_include_directories(network PRIVATE ${NETWORK_INCLUDE_DIRS})

target_link_libraries(network PRIVATE
    ${Protobuf_LIBRARIES}          # 直接链接 Protobuf 3.19.5 静态库
    ${Boost_LIBRARIES}
    ZLIB::ZLIB
)

# Windows 需要额外链接 winsock
if(WIN32)
    target_link_libraries(network PRIVATE ws2_32)
endif()

# macOS：开发阶段编译当前架构即可，发布时再生成 universal binary
# 如需 universal binary，取消注释下面两行并安装多架构版本的依赖库
# if(APPLE)
#     set_target_properties(network PROPERTIES OSX_ARCHITECTURES "x86_64;arm64")
# endif()

# ============================================================
# 编译产物输出路径
# ============================================================
set_target_properties(network PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)
