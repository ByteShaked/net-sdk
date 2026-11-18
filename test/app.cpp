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
#include <openssl/md5.h>
#include <iomanip>
#include <sstream>
#include <ctime>

// 全局运行标志和网络句柄
std::atomic<bool> running(true);
NetworkHandle g_handle = nullptr;

// 生成当前时间戳 (格式: YYYYMMDDHHmmss)
std::string getFormattedTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_info->tm_year + 1900)
        << std::setw(2) << (tm_info->tm_mon + 1)
        << std::setw(2) << tm_info->tm_mday
        << std::setw(2) << tm_info->tm_hour
        << std::setw(2) << tm_info->tm_min
        << std::setw(2) << tm_info->tm_sec;

    return oss.str();
}

// 计算认证签名: MD5(appId + username + timestamp + appToken)
std::string calculateSig(const std::string& appId,
                        const std::string& username,
                        const std::string& timestamp,
                        const std::string& appToken) {
    std::string input = appId + username + timestamp + appToken;

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.c_str(), input.length(), digest);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }

    return oss.str();
}

// 信号处理函数
void signal_handler(int signal) {
    std::cout << "\n正在关闭程序..." << std::endl;
    running = false;
}

// 数据回调：打印接收到的数据
void on_data(const char* data, int len, void* userdata) {
    std::cout << "\n========== 收到服务器响应 ==========" << std::endl;
    std::cout << "数据长度: " << len << " 字节" << std::endl;

    tutorial::Person person;
    const std::string serialized_str(data, len);
    if (person.ParseFromString(serialized_str)) {
        std::cout << "消息类型 (prototype): " << person.prototype() << std::endl;
        std::cout << "错误码 (protoerrorcode): " << person.protoerrorcode() << std::endl;
        std::cout << "客户端编号 (protoclientno): " << person.protoclientno() << std::endl;

        if (person.prototype() == 20) {
            std::cout << ">>> 认证响应收到！" << std::endl;

            // 解析认证响应的内层数据
            if (!person.protodata().empty()) {
                // protodata 格式：[varint长度] + [实际数据]
                // 需要先跳过 varint 长度前缀
                const std::string& proto_data = person.protodata();
                size_t offset = 0;

                // 简单跳过 varint（假设长度 <= 127，varint 占 1 字节）
                if (proto_data.size() > 0) {
                    uint8_t first_byte = static_cast<uint8_t>(proto_data[0]);
                    if (first_byte & 0x80) {
                        // 多字节 varint，跳过直到找到最高位为 0 的字节
                        while (offset < proto_data.size() && (static_cast<uint8_t>(proto_data[offset]) & 0x80)) {
                            offset++;
                        }
                        offset++; // 跳过最后一个字节
                    } else {
                        // 单字节 varint
                        offset = 1;
                    }
                }

                std::string inner_data = proto_data.substr(offset);
                tutorial::UserAuthInner authResp;
                if (authResp.ParseFromString(inner_data)) {
                    std::cout << "\n认证响应详情：" << std::endl;
                    std::cout << "  authType: " << authResp.authtype() << std::endl;
                    std::cout << "  username: " << authResp.username() << std::endl;
                    std::cout << "  sdkVersion: " << authResp.sdkversion() << std::endl;
                    std::cout << "  mode: " << authResp.mode() << std::endl;
                    if (!authResp.extopts().empty()) {
                        std::cout << "  extOpts: " << authResp.extopts() << std::endl;
                    }
                } else {
                    std::cerr << "  [警告] 解析认证响应内层数据失败" << std::endl;
                    std::cerr << "  protodata 前10字节 (hex): ";
                    for (size_t i = 0; i < std::min(proto_data.size(), size_t(10)); i++) {
                        printf("%02x ", (uint8_t)proto_data[i]);
                    }
                    std::cerr << std::endl;
                }
            }
        } else if (person.prototype() == 29) {
            std::cout << ">>> 消息发送响应收到！" << std::endl;
        }

        if (!person.protodata().empty()) {
            std::cout << "响应数据长度: " << person.protodata().length() << " 字节" << std::endl;
        }
    } else {
        std::cerr << "解析响应失败" << std::endl;
    }
    std::cout << "==================================\n" << std::endl;
}

// 错误回调：打印错误信息
void on_error(int code, const char* msg, void* userdata) {
    std::cerr << "[错误 " << code << "] " << msg << std::endl;
}

// 发送认证消息（封装成函数，暂不调用）
void sendAuthMessage() {
    // 认证参数（从鸿蒙抓取）
    std::string appId = "cba0bb291548a8d73fa5502bebe4e64d";
    std::string username = "cjcjzqwbcs1";
    std::string appToken = "fba8a76c763639b0b9a2d96ff1915877";

    // 生成当前时间戳
    std::string timestamp = getFormattedTimestamp();

    // 计算签名
    std::string sig = calculateSig(appId, username, timestamp, appToken);

    std::cout << "\n========== 发送认证消息 ==========" << std::endl;
    std::cout << "生成认证参数:" << std::endl;
    std::cout << "  appId: " << appId << std::endl;
    std::cout << "  username: " << username << std::endl;
    std::cout << "  timestamp: " << timestamp << std::endl;
    std::cout << "  appToken: " << appToken << std::endl;
    std::cout << "  sig: " << sig << std::endl;

    tutorial::UserAuthInner userAuth;
    userAuth.set_authtype(1);  // loginType: NORMAL_AUTH
    userAuth.set_appid(appId);
    userAuth.set_username(username);
    userAuth.set_timestamp(timestamp);
    userAuth.set_device(1);
    userAuth.set_sig(sig);
    userAuth.set_sdkversion("6.2.0");
    userAuth.set_imei("bf77a0fef33956721f1404ea8a3a60ad");
    userAuth.set_mode(2);  // loginMode: AUTO
    userAuth.set_network(1);
    userAuth.set_pwd("");
    userAuth.set_bundleid("1");
    userAuth.set_subdevice("HUAWEI");
    userAuth.set_language("zh_CN");
    userAuth.set_atmid("");
    userAuth.set_clientip("");
    userAuth.set_extopts("");

    std::string authData;
    userAuth.SerializeToString(&authData);

    int ret = network_send(g_handle, authData.c_str(), authData.length(), 20, 0);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "认证发送失败 (错误码 " << ret << ")" << std::endl;
    } else {
        std::cout << "认证消息已发送，等待响应..." << std::endl;
    }
    std::cout << "==================================\n" << std::endl;
}

// 发送聊天消息（封装成函数，暂不调用）
void sendChatMessage() {
    std::cout << "\n========== 发送聊天消息 ==========" << std::endl;

    tutorial::SendMsgInner sendMsg;
    sendMsg.set_msgtype(1);
    sendMsg.set_msgid("");
    sendMsg.set_msgcontent("hello from macOS test");
    sendMsg.set_msgsender("");
    sendMsg.set_msgreceiver("zczb45911");

    std::string msgData;
    sendMsg.SerializeToString(&msgData);

    std::cout << "正在发送消息到: zczb45911" << std::endl;
    int ret = network_send(g_handle, msgData.c_str(), msgData.length(), 29, 0);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "消息发送失败 (错误码 " << ret << ")" << std::endl;
    } else {
        std::cout << "聊天消息已发送" << std::endl;
    }
    std::cout << "==================================\n" << std::endl;
}

int main() {
    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#ifdef _WIN32
    // Windows 需要初始化 Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // 1. 创建网络句柄
    g_handle = network_create();
    if (!g_handle) {
        std::cerr << "创建网络句柄失败" << std::endl;
        return 1;
    }

    // 2. 设置回调
    network_set_callbacks(g_handle, on_data, on_error, nullptr);

    // 3. 连接服务器
    const char* host = "202.103.39.205";  // 使用鸿蒙抓取的服务器IP
    int port = 8086;  // 使用鸿蒙抓取的端口

    std::cout << "正在连接 " << host << ":" << port << "..." << std::endl;

    ConnectOptionsC options = {0};
    options.host = host;
    options.port = port;
    options.max_retry_count = 3;
    options.retry_interval_sec = 2;
    options.auto_reconnect = true;
    options.heartbeat_interval_sec = 5;  // 改为 5 秒，与鸿蒙一致

    int ret = network_connect(g_handle, &options);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "连接失败 (错误码 " << ret << ")" << std::endl;
        network_destroy(g_handle);
        return 1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "连接成功！" << std::endl;
    std::cout << "接收线程和心跳线程已自动启动" << std::endl;
    std::cout << "========================================" << std::endl;

    // 连接后立即发送认证消息
    sendAuthMessage();

    std::cout << "\n提示：" << std::endl;
    std::cout << "  - 可以手动调用 sendChatMessage() 发送消息" << std::endl;
    std::cout << "  - 按 Ctrl+C 退出程序" << std::endl;
    std::cout << "\n程序运行中...\n" << std::endl;

    // 主循环：保持程序运行，观察心跳日志
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 清理资源
    std::cout << "\n正在关闭连接..." << std::endl;
    network_disconnect(g_handle);

    // 等待线程清理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    network_destroy(g_handle);

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "程序已退出" << std::endl;
    return 0;
}
