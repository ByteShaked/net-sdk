#include <network_api.h>
#include <iostream>
#include <cstring>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../proto_v19/msg2lite.pb.h"
#include "../proto_v19/UserAuth.pb.h"
#include "../proto_v19/SendMsg.pb.h"
#include "../include/TlvHandler.h"
// 全局运行标志和网络句柄
std::atomic<bool> running(true);
NetworkHandle g_handle = nullptr;

// 自定义信号处理函数
void signal_handler(int signal) {
    std::cout << "\nShutting down..." << std::endl;
    running = false;
}

// 数据回调：打印接收到的数据
void on_data(const char* data, int len, void* userdata) {
    std::cout << "Received data (length: " << len << " bytes)" << std::endl;
    std::cout << "Received data : " << data << std::endl;
    tutorial::Person person;
    const std::string serialized_str(data);
    person.ParseFromString(serialized_str);
    std::cout << "prototype: " << person.prototype() << std::endl;
    std::cout << "protoerrorcode: " << person.protoerrorcode() << std::endl;
    std::cout << "protodata: " << person.protodata() << std::endl;

}

// 错误回调：打印错误信息
void on_error(int code, const char* msg, void* userdata) {
    std::cerr << "[ERROR " << code << "] " << msg << std::endl;
}

int main() {
    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    tutorial::UserAuthInner UserAuthInner;
    UserAuthInner.set_authtype(1);
    UserAuthInner.set_appid("2466fc0c84220a57ecb7f0c9d6ad2460");
    UserAuthInner.set_username("zc68zctcs2");
    UserAuthInner.set_timestamp("20250409152312");
    UserAuthInner.set_device(32);
    UserAuthInner.set_sig("F9CEE7FB78E9C0355412178293209D18");
    UserAuthInner.set_sdkversion("8.0.1");
    UserAuthInner.set_imei("1234567890");
    UserAuthInner.set_mode(1);
    UserAuthInner.set_network(1);
    UserAuthInner.set_pwd("");
    UserAuthInner.set_bundleid("1");
    UserAuthInner.set_subdevice("huawei");
    UserAuthInner.set_language("zh_CN");
    UserAuthInner.set_atmid("");
    UserAuthInner.set_clientip("");
    UserAuthInner.set_extopts("");

   std::string inner_serialized_str;
    UserAuthInner.SerializeToString(&inner_serialized_str);
    // 创建一个 Person 对象
    tutorial::Person MsgLiteInner;
    MsgLiteInner.set_prototype(20);
    MsgLiteInner.set_protodata(inner_serialized_str);
    // 1. 创建网络句柄
    g_handle = network_create();
    if (!g_handle) {
        std::cerr << "Failed to create network handle" << std::endl;
        return 1;
    }

    // 2. 设置回调
    network_set_callbacks(g_handle, on_data, on_error, nullptr);

    char* ip = new char[256];
	struct hostent *host1 = gethostbyname("zctxc.crrcgc.cc");
	if (host1 == NULL)
		return 1;
	for (int i = 0; host1->h_addr_list[i]; i++)
	{
		strcpy(ip, inet_ntoa(*(struct in_addr*)host1->h_addr_list[i]));
		break;
	}
    std::cout << "ip: " << ip << std::endl;
    // 3. 连接服务器
    const char* host = "223.78.68.146";
    int port = 8085;
    int ret = network_connect(g_handle, host, port);
    if (ret != NETWORK_OK) {
        std::cerr << "Connect failed (code " << ret << ")" << std::endl;
        network_destroy(g_handle);
        return 1;
    }

    // 4. 启动心跳（每10秒一次）
    ret = network_start_heartbeat(g_handle, 30);
    if (ret != HEARTBEAT_OK) {
        std::cerr << "Failed to start heartbeat (code " << ret << ")" << std::endl;
    }

    // 5. 发送测试消息
    ret = network_send(g_handle, inner_serialized_str.c_str(), inner_serialized_str.length(), 20, 0);
    if (ret != NETWORK_OK) {
        std::cerr << "Send failed (code " << ret << ")" << std::endl;
    }
    // for(int i=0;i<5;i++)
    {
    tutorial::SendMsgInner SendMsgInner;
    SendMsgInner.set_msgtype(1);
    SendMsgInner.set_msgid("");
    SendMsgInner.set_msgcontent("hello");
    SendMsgInner.set_msgsender("");
    SendMsgInner.set_msgreceiver("zc68zctcs3");

    std::string inner_serialized_str_send_msg;
    SendMsgInner.SerializeToString(&inner_serialized_str_send_msg);
        sleep(3);
    ret = network_send(g_handle, inner_serialized_str_send_msg.c_str(), inner_serialized_str_send_msg.length(), 29, 0);
    if (ret != NETWORK_OK) {
        std::cerr << "Send failed (code " << ret << ")" << std::endl;
    }
}
    // 主循环：保持程序运行
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 6. 清理资源
    std::cout << "Stopping heartbeat..." << std::endl;
    network_stop_heartbeat(g_handle);

    std::cout << "Disconnecting..." << std::endl;
    network_disconnect(g_handle);

    std::cout << "Destroying handle..." << std::endl;
    network_destroy(g_handle);

    return 0;
}