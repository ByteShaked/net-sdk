#include "network_api.h"
#include <iostream>
#include <cstring>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// 平台特定头文件（DNS 解析用，实际上 network_connect 内部已处理，这里保留仅供参考）
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

// proto 头文件（已统一路径）
#include "../include/TlvHandler.h"  // 包含 Person 别名定义和 proto 头文件
#include "../proto_v19/auth/UserAuth.pb.h"
#include "../proto_v19/msg/SendMsg.pb.h"

// 全局运行标志和网络句柄
std::atomic<bool> running(true);
NetworkHandle g_handle = nullptr;

// 信号处理函数
void signal_handler(int signal) {
    std::cout << "\nShutting down..." << std::endl;
    running = false;
}

// 数据回调：打印接收到的数据
void on_data(const char* data, int len, void* userdata) {
    std::cout << "Received data (length: " << len << " bytes)" << std::endl;

    tutorial::Person person;
    const std::string serialized_str(data, len);
    if (person.ParseFromString(serialized_str)) {
        std::cout << "prototype: " << person.prototype() << std::endl;
        std::cout << "protoerrorcode: " << person.protoerrorcode() << std::endl;
        std::cout << "protodata: " << person.protodata() << std::endl;
    } else {
        std::cerr << "Failed to parse response" << std::endl;
    }
}

// 错误回调：打印错误信息
void on_error(int code, const char* msg, void* userdata) {
    std::cerr << "[ERROR " << code << "] " << msg << std::endl;
}

int main() {
    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#ifdef _WIN32
    // Windows 需要初始化 Winsock（如果 network_api 内部没做的话）
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // 1. 创建网络句柄
    g_handle = network_create();
    if (!g_handle) {
        std::cerr << "Failed to create network handle" << std::endl;
        return 1;
    }

    // 2. 设置回调
    network_set_callbacks(g_handle, on_data, on_error, nullptr);

    // 3. 连接服务器（DNS 解析由 network_connect 内部完成）
    const char* host = "223.78.68.146";  // 或使用域名 "zctxc.crrcgc.cc"
    int port = 8085;

    std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;

    ConnectOptionsC options = {0};
    options.host = host;
    options.port = port;
    options.max_retry_count = 3;
    options.retry_interval_sec = 2;
    options.auto_reconnect = false;
    options.heartbeat_interval_sec = 30;

    int ret = network_connect(g_handle, &options);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "Connect failed (code " << ret << ")" << std::endl;
        network_destroy(g_handle);
        return 1;
    }
    std::cout << "Connected successfully!" << std::endl;

    // 4. 启动心跳（每30秒一次）
    ret = network_start_heartbeat(g_handle, 30);
    if (ret != HEARTBEAT_OK) {
        std::cerr << "Failed to start heartbeat (code " << ret << ")" << std::endl;
    }

    // 等待连接稳定
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 5. 发送认证消息
    tutorial::UserAuthInner userAuth;
    userAuth.set_authtype(1);
    userAuth.set_appid("2466fc0c84220a57ecb7f0c9d6ad2460");
    userAuth.set_username("zc68zctcs2");
    userAuth.set_timestamp("20250409152312");
    userAuth.set_device(32);
    userAuth.set_sig("F9CEE7FB78E9C0355412178293209D18");
    userAuth.set_sdkversion("8.0.1");
    userAuth.set_imei("1234567890");
    userAuth.set_mode(1);
    userAuth.set_network(1);
    userAuth.set_pwd("");
    userAuth.set_bundleid("1");
    userAuth.set_subdevice("huawei");
    userAuth.set_language("zh_CN");
    userAuth.set_atmid("");
    userAuth.set_clientip("");
    userAuth.set_extopts("");

    std::string authData;
    userAuth.SerializeToString(&authData);

    std::cout << "Sending authentication..." << std::endl;
    ret = network_send(g_handle, authData.c_str(), authData.length(), 20, 0);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "Auth send failed (code " << ret << ")" << std::endl;
    }

    // 等待认证响应
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 6. 发送聊天消息
    tutorial::SendMsgInner sendMsg;
    sendMsg.set_msgtype(1);
    sendMsg.set_msgid("");
    sendMsg.set_msgcontent("hello from cross-platform test");
    sendMsg.set_msgsender("");
    sendMsg.set_msgreceiver("zc68zctcs3");

    std::string msgData;
    sendMsg.SerializeToString(&msgData);

    std::cout << "Sending chat message..." << std::endl;
    ret = network_send(g_handle, msgData.c_str(), msgData.length(), 29, 0);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "Message send failed (code " << ret << ")" << std::endl;
    }

    // 主循环：保持程序运行
    std::cout << "Running... (Press Ctrl+C to exit)" << std::endl;
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 7. 清理资源
    std::cout << "Stopping heartbeat..." << std::endl;
    network_stop_heartbeat(g_handle);

    std::cout << "Disconnecting..." << std::endl;
    network_disconnect(g_handle);

    std::cout << "Destroying handle..." << std::endl;
    network_destroy(g_handle);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
