#include "network_api.h"
#include <iostream>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

// 平台特定头文件
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

// proto 头文件
#include "../include/TlvHandler.h"
#include "../proto_v19/auth/UserAuth.pb.h"
#include "../proto_v19/auth/UserAuthResp.pb.h"
#include <openssl/md5.h>
#include <iomanip>
#include <sstream>
#include <ctime>

// CI 测试专用标志
std::atomic<bool> auth_received(false);
std::atomic<bool> auth_success(false);
std::atomic<bool> test_timeout(false);
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

// Varint 解码函数
std::pair<uint32_t, size_t> decode_varint(const std::vector<uint8_t>& buffer, size_t pos) {
    uint32_t result = 0;
    size_t bytes_read = 0;
    uint32_t shift = 0;

    while (true) {
        if (pos + bytes_read >= buffer.size()) {
            return std::make_pair(0, 0);
        }
        uint8_t byte = buffer[pos + bytes_read];
        result |= (byte & 0x7F) << shift;
        bytes_read++;
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
        if (shift >= 32) {
            return std::make_pair(0, 0);
        }
    }
    return std::make_pair(result, bytes_read);
}

// 数据回调：CI 模式 - 只检查认证响应
void on_data(const char* data, int len, void* userdata) {
    std::cout << "\n[CI TEST] 收到数据 (长度: " << len << " 字节)" << std::endl;

    // 解析外层协议
    tutorial::Person outer;
    if (!outer.ParseFromArray(data, len)) {
        std::cerr << "[CI TEST] ❌ 外层协议解析失败" << std::endl;
        return;
    }

    std::cout << "[CI TEST] 消息类型: " << outer.prototype() << std::endl;
    std::cout << "[CI TEST] 错误码: " << outer.protoerrorcode() << std::endl;

    // 只处理认证响应 (type 20)
    if (outer.prototype() == 20) {
        if (!outer.protodata().empty()) {
            const std::string& protoData = outer.protodata();
            std::vector<uint8_t> buffer(protoData.begin(), protoData.end());

            // 解码 varint
            auto result_varint = decode_varint(buffer, 0);
            uint32_t innerLen = result_varint.first;
            uint32_t varintBytes = result_varint.second;

            if (varintBytes > 0 && buffer.size() >= varintBytes + innerLen) {
                tutorial::UserAuthRespInner authResp;
                if (authResp.ParseFromArray(buffer.data() + varintBytes, innerLen)) {
                    std::cout << "[CI TEST] ✅ 认证响应解析成功" << std::endl;
                    std::cout << "[CI TEST] authState: " << authResp.authstate() << std::endl;
                    std::cout << "[CI TEST] connectorId: " << authResp.connectorid() << std::endl;

                    auth_received = true;
                    // authState == 1 表示认证成功
                    auth_success = (authResp.authstate() == 1);
                } else {
                    std::cerr << "[CI TEST] ❌ 认证响应内层解析失败" << std::endl;
                }
            }
        }
    }
}

// 错误回调
void on_error(int code, const char* msg, void* userdata) {
    std::cerr << "[CI TEST] ❌ 错误 [" << code << "]: " << msg << std::endl;
}

// 发送认证消息
void sendAuthMessage() {
    std::string appId = "cba0bb291548a8d73fa5502bebe4e64d";
    std::string username = "cjcjzqwbcs1";
    std::string appToken = "fba8a76c763639b0b9a2d96ff1915877";
    std::string timestamp = getFormattedTimestamp();
    std::string sig = calculateSig(appId, username, timestamp, appToken);

    std::cout << "\n[CI TEST] 发送认证消息" << std::endl;
    std::cout << "[CI TEST] appId: " << appId << std::endl;
    std::cout << "[CI TEST] username: " << username << std::endl;
    std::cout << "[CI TEST] timestamp: " << timestamp << std::endl;

    tutorial::UserAuthInner userAuth;
    userAuth.set_authtype(1);
    userAuth.set_appid(appId);
    userAuth.set_username(username);
    userAuth.set_timestamp(timestamp);
    userAuth.set_device(1);
    userAuth.set_sig(sig);
    userAuth.set_sdkversion("6.2.0");
    userAuth.set_imei("bf77a0fef33956721f1404ea8a3a60ad");
    userAuth.set_mode(1);
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
        std::cerr << "[CI TEST] ❌ 认证发送失败 (错误码 " << ret << ")" << std::endl;
    } else {
        std::cout << "[CI TEST] ✅ 认证消息已发送" << std::endl;
    }
}

// 超时监控线程
void timeout_watchdog(int timeout_seconds) {
    for (int i = 0; i < timeout_seconds; i++) {
        if (auth_received) {
            return;  // 已收到响应，退出监控
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    test_timeout = true;
    std::cerr << "[CI TEST] ⏱️ 超时：" << timeout_seconds << " 秒内未收到认证响应" << std::endl;
}

int main() {
    // 禁用标准输出缓冲，确保日志即时打印
    std::cout.setf(std::ios::unitbuf);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::cout << "========================================" << std::endl;
    std::cout << "[CI TEST] Windows x64 网络库测试开始" << std::endl;
    std::cout << "========================================\n" << std::endl;

#ifdef _WIN32
    // Windows 需要初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[CI TEST] ❌ Winsock 初始化失败" << std::endl;
        return 1;
    }
    std::cout << "[CI TEST] ✅ Winsock 初始化成功" << std::endl;
#endif

    // 1. 创建网络句柄
    std::cout << "\n[CI TEST] 步骤 1: 创建网络句柄" << std::endl;
    g_handle = network_create();
    if (!g_handle) {
        std::cerr << "[CI TEST] ❌ 创建网络句柄失败" << std::endl;
        return 1;
    }
    std::cout << "[CI TEST] ✅ 网络句柄创建成功" << std::endl;

    // 2. 设置回调
    std::cout << "\n[CI TEST] 步骤 2: 设置回调函数" << std::endl;
    network_set_callbacks(g_handle, on_data, on_error, nullptr);
    std::cout << "[CI TEST] ✅ 回调函数设置成功" << std::endl;

    // 3. 连接服务器
    const char* host = "202.103.39.205";
    int port = 8086;

    std::cout << "\n[CI TEST] 步骤 3: 连接服务器" << std::endl;
    std::cout << "[CI TEST] 服务器: " << host << ":" << port << std::endl;

    ConnectOptionsC options = {0};
    options.host = host;
    options.port = port;
    options.max_retry_count = 3;
    options.retry_interval_sec = 2;
    options.auto_reconnect = false;  // CI 模式不自动重连
    options.heartbeat_interval_sec = 10;

    int ret = network_connect(g_handle, &options);
    if (ret != NETWORK_SUCCESS) {
        std::cerr << "[CI TEST] ❌ 连接失败 (错误码 " << ret << ")" << std::endl;
        network_destroy(g_handle);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    std::cout << "[CI TEST] ✅ 连接成功" << std::endl;

    // 4. 发送认证消息
    std::cout << "\n[CI TEST] 步骤 4: 发送认证消息" << std::endl;
    sendAuthMessage();

    // 5. 启动超时监控（15 秒超时）
    std::cout << "\n[CI TEST] 步骤 5: 等待认证响应 (超时 15 秒)" << std::endl;
    std::thread watchdog(timeout_watchdog, 15);

    // 6. 等待认证响应或超时
    while (!auth_received && !test_timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    watchdog.join();

    // 7. 检查测试结果
    std::cout << "\n========================================" << std::endl;
    std::cout << "[CI TEST] 测试结果" << std::endl;
    std::cout << "========================================" << std::endl;

    int exit_code = 0;

    if (test_timeout) {
        std::cerr << "[CI TEST] ❌ 测试失败: 超时" << std::endl;
        exit_code = 2;
    } else if (!auth_received) {
        std::cerr << "[CI TEST] ❌ 测试失败: 未收到响应" << std::endl;
        exit_code = 3;
    } else if (!auth_success) {
        std::cerr << "[CI TEST] ❌ 测试失败: 认证未成功" << std::endl;
        exit_code = 4;
    } else {
        std::cout << "[CI TEST] ✅ 测试成功: 库可正常工作" << std::endl;
        std::cout << "[CI TEST] - 连接建立成功" << std::endl;
        std::cout << "[CI TEST] - 数据发送成功" << std::endl;
        std::cout << "[CI TEST] - 数据接收成功" << std::endl;
        std::cout << "[CI TEST] - 协议解析成功" << std::endl;
        exit_code = 0;
    }

    // 8. 清理资源
    std::cout << "\n[CI TEST] 清理资源..." << std::endl;
    network_disconnect(g_handle);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    network_destroy(g_handle);

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "[CI TEST] 程序退出 (退出码: " << exit_code << ")" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return exit_code;
}

