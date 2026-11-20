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
# 鸿蒙交叉编译时通过 CMake 参数覆盖这些路径
if(NOT Protobuf_INCLUDE_DIR)
    set(CMAKE_PREFIX_PATH "/opt/protobuf-3.19.5" ${CMAKE_PREFIX_PATH})
    set(Protobuf_INCLUDE_DIR "/opt/protobuf-3.19.5/include")
    set(Protobuf_LIBRARIES "/opt/protobuf-3.19.5/lib/libprotobuf.a")
    set(Protobuf_PROTOC_EXECUTABLE "/opt/protobuf-3.19.5/bin/protoc")
endif()

# 查找 Boost（鸿蒙交叉编译时可选）
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    message(STATUS "Cross-compiling for OpenHarmony, Boost not required")
    set(SKIP_BOOST TRUE)
else()
    find_package(Boost REQUIRED COMPONENTS system)
    set(SKIP_BOOST FALSE)
endif()

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
)

# 非鸿蒙平台才添加 Boost 头文件
if(NOT SKIP_BOOST)
    list(APPEND NETWORK_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
endif()

# ============================================================
# 编译动态库 libnetwork
# ============================================================
add_library(network SHARED ${NETWORK_SOURCES})

# Windows 平台使用静态运行时库（与 Protobuf 3.19.5 保持一致）
if(MSVC)
    set_property(TARGET network PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(network PRIVATE ${NETWORK_INCLUDE_DIRS})

# 根据平台链接不同的库
if(SKIP_BOOST)
    # 鸿蒙交叉编译：不链接 Boost
    target_link_libraries(network PRIVATE
        ${Protobuf_LIBRARIES}          # 直接链接 Protobuf 3.19.5 静态库
        ZLIB::ZLIB
    )
else()
    # 其他平台：链接 Boost
    target_link_libraries(network PRIVATE
        ${Protobuf_LIBRARIES}          # 直接链接 Protobuf 3.19.5 静态库
        ${Boost_LIBRARIES}
        ZLIB::ZLIB
    )
endif()

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

# ============================================================
# CI 测试可执行文件（可选，通过 -DBUILD_TESTS=ON 启用）
# ============================================================
option(BUILD_TESTS "Build test executables" OFF)

if(BUILD_TESTS)
    message(STATUS "Building CI test executable")

    # 查找 OpenSSL（测试用例需要 MD5）
    find_package(OpenSSL REQUIRED)

    add_executable(test_ci_windows
        ${CMAKE_CURRENT_SOURCE_DIR}/test/app_ci_windows.cpp
        ${PROTO_SOURCES}
    )

    # Windows 平台使用静态运行时库
    if(MSVC)
        set_property(TARGET test_ci_windows PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    target_include_directories(test_ci_windows PRIVATE
        ${NETWORK_INCLUDE_DIRS}
        ${Protobuf_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
    )

    target_link_libraries(test_ci_windows PRIVATE
        network
        ${Protobuf_LIBRARIES}
        ZLIB::ZLIB
        OpenSSL::Crypto
    )

    # Windows 需要额外链接 winsock
    if(WIN32)
        target_link_libraries(test_ci_windows PRIVATE ws2_32)
    endif()

    set_target_properties(test_ci_windows PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build-windows/Release
    )
endif()
