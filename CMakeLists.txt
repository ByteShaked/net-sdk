CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(im_network)
# 查找 OpenSSL 包
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0)
message(gcc version too high, Unable to guarantee successful compilation)
endif()
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Add defined macro
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D_LOG4CXX_SPI_LOCATION_LOCATIONINFO_H -std=c++11")
# Set some values
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2 -fPIC -Wl,-z,now -Wl,-z,relro -fPIE -pie -fstack-protector-all -D_FORTIFY_SOURCE=2")
SET(CMAKE_SKIP_RPATH TRUE)

SET(ROOT ${CMAKE_CURRENT_SOURCE_DIR})
SET(EXECUTABLE_OUTPUT_PATH ${ROOT}/bin)
set(LIBRARY_OUTPUT_PATH ${ROOT}/lib)

if(BUILD_VERSION)
    add_definitions(-DBUILD_VERSION=${BUILD_VERSION})
else()
    add_definitions(-DBUILD_VERSION=0)
endif()

if(MAJOR_VERSION)
    add_definitions(-DMAJOR_VERSION=${MAJOR_VERSION})
else()
    add_definitions(-DMAJOR_VERSION=1)
endif()

if(MINOR_VERSION)
    add_definitions(-DMINOR_VERSION=${MINOR_VERSION})
else()
    add_definitions(-DMINOR_VERSION=0)
endif()

if(RELEASE_VERSION)
    add_definitions(-DRELEASE_VERSION=${RELEASE_VERSION})
else()
    add_definitions(-DRELEASE_VERSION=0)
endif()
add_definitions(-D LINUX)

SET(PROTO /usr/local/protobuf_v19/)
SET(BOOST /usr/local/boost/)

SET(TARGET ${PROJECT_NAME})

# Include path
INCLUDE_DIRECTORIES(
    ${ROOT}/include
    ${ROOT}/
    ${PROTO}/include
    ${BOOST}/include

)

# Lib path
LINK_DIRECTORIES(
    ${ROOT}/lib
    ${PROTO}/lib
    ${BOOST}/lib/
)

# Libraries
SET(PROJECT_LIBRARY

)

FILE(GLOB_RECURSE CPPFiles ${ROOT}/src/*.cpp ${ROOT}/src/*.c  ${ROOT}/proto/*.cc ${ROOT}/include/*.hpp ${ROOT}/include/*.h  ${ROOT}/proto/*.pb.h)

ADD_EXECUTABLE(${PROJECT_NAME} ${CPPFiles})

target_link_libraries(${PROJECT_NAME} ${PROJECT_LIBRARY})

# C接口动态库
add_library(network SHARED ${ROOT}/src/network_api.cpp ${ROOT}/src/crypto_util.cpp ${ROOT}/src/NetworkModule.cpp ${ROOT}/src/TlvHandler.cpp ${ROOT}/proto/msg2lite.pb.cc ${ROOT}/include/common.h ${ROOT}/include/crypto_util.h ${ROOT}/include/NetworkModule.h ${ROOT}/include/TlvHandler.h ${ROOT}/proto/msg2lite.pb.h)