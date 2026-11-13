#include "network_api.h"
#include <iostream>

int main() {
    std::cout << "=== Minimal Network Library Test ===" << std::endl;

    NetworkHandle handle = network_create();
    std::cout << "Handle: " << (handle ? "OK" : "FAIL") << std::endl;

    auto on_data = [](const char* data, int len, void* userdata) {
        std::cout << "Data callback: " << len << " bytes" << std::endl;
    };
    auto on_error = [](int code, const char* msg, void* userdata) {
        std::cout << "Error callback: [" << code << "] " << msg << std::endl;
    };
    network_set_callbacks(handle, on_data, on_error, nullptr);

    // 连接本地测试服务器
    ConnectOptionsC options = {};
    options.host = "127.0.0.1";
    options.port = 9999;
    options.max_retry_count = 0;
    options.retry_interval_sec = 1;
    options.auto_reconnect = 0;
    options.heartbeat_interval_sec = 5;

    std::cout << "Connecting to 127.0.0.1:9999..." << std::endl;
    int ret = network_connect(handle, &options);
    std::cout << "network_connect returned: " << ret << std::endl;

    if (ret == NETWORK_SUCCESS) {
        std::cout << "PASS: Connected to local server!" << std::endl;
    } else {
        std::cout << "FAIL: code=" << ret << std::endl;
    }

    network_destroy(handle);
    std::cout << "Done." << std::endl;
    return 0;
}
