#include "network_api.h"
#include "NetworkModule.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "common.h"
// 全局实例管理
static std::map<NetworkHandle, tutorial::NetworkModule*> instances;
static std::mutex instance_mutex;

// 转换16进制字符串为字节数组
static std::vector<uint8_t> hex_to_bytes(const char* hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; hex[i] && hex[i+1]; i += 2) {
        bytes.push_back(static_cast<uint8_t>(strtoul(hex + i, nullptr, 16)));
    }
    return bytes;
}

// 接口实现
NetworkHandle network_create() {
    std::lock_guard<std::mutex> lock(instance_mutex);
    
    try {
        auto* core = new tutorial::NetworkModule();
        NetworkHandle handle = reinterpret_cast<NetworkHandle>(core);
        instances[handle] = core;
        return handle;
    } catch (...) {
        return nullptr;
    }
}

void network_destroy(NetworkHandle handle) {
    tutorial::NetworkModule* module = nullptr;
    {
        std::lock_guard<std::mutex> lock(instance_mutex);
        auto it = instances.find(handle);
        if (it != instances.end()) {
            module = it->second;
            instances.erase(it);
        }
    }
    if (module) {
        module->destroy();
        delete module;
    }
}

int network_connect(NetworkHandle handle, const ConnectOptionsC* options) {
    if (!handle || !options) return NETWORK_ERR_INVALID_HANDLE;
    tutorial::ConnectOptions cppOpt;
    cppOpt.host = options->host;
    cppOpt.port = options->port;
    cppOpt.max_retry_count = options->max_retry_count;
    cppOpt.retry_interval_sec = options->retry_interval_sec;
    cppOpt.auto_reconnect = options->auto_reconnect;
    cppOpt.heartbeat_interval_sec = options->heartbeat_interval_sec;
    auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
    return core->connect(cppOpt) ? NETWORK_SUCCESS : NETWORK_ERR_CONNECT_FAILED;
}

void network_disconnect(NetworkHandle handle) {
    auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
    if (core) core->disconnect();
}

int network_send(NetworkHandle handle, 
                const char* data, 
                int len,
                int msg_type,
                int encrypt) 
{
    auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
    if (!core || !data || len <= 0) return NETWORK_ERR_INVALID_PARAM;

    try {
        tutorial::Person outer;
        outer.set_prototype(msg_type);
        outer.set_protodata(data, len);
        outer.set_protoencrypt(encrypt);
        core->sendMessage(tutorial::ProtocolType::TOKEN_AUTH,outer);
        return NETWORK_SUCCESS;
    } catch (...) {
        return NETWORK_ERR_SEND_EXCEPTION;
    }
}

void network_set_callbacks(NetworkHandle handle,
                          DataCallback data_cb,
                          ErrorCallback error_cb,
                          void* userdata) 
{
    auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
    if (!core) return;

    core->setDataCallback([=](const std::vector<uint8_t>& data) {
        if (data_cb) {
            data_cb(reinterpret_cast<const char*>(data.data()), 
                    data.size(),
                    userdata);
        }
    });

    core->setErrorCallback([=](int code, const std::string& msg) {
        if (error_cb) {
            error_cb(code, msg.c_str(), userdata);
        }
    });
}
// 新增心跳接口实现
// --------------------------
int network_start_heartbeat(NetworkHandle handle, int interval_sec) {
    auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
    if (!core) return NETWORK_ERR_INVALID_HANDLE;
    if (interval_sec <= 0) return NETWORK_ERR_HEARTBEAT_INVALID_INTERVAL;
    
    try {
        core->setHeartbeatInterval(interval_sec);
        core->startHeartbeat();
        return NETWORK_SUCCESS;
    } catch (...) {
        return NETWORK_ERR_HEARTBEAT_FAILED;
    }
}

// int network_set_heartbeat_interval(NetworkHandle handle, int interval_sec) {
//     auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
//     if (!core) return NETWORK_ERR_INVALID_HANDLE;
//     if (interval_sec <= 0) return HEARTBEAT_ERR_INVALID_INTERVAL;
    
//     try {
//         core->setHeartbeatInterval(interval_sec);
//         return HEARTBEAT_OK;
//     } catch (...) {
//         return NETWORK_ERR_HEARTBEAT_FAILED;
//     }
// }

int network_stop_heartbeat(NetworkHandle handle) {
    auto* core = reinterpret_cast<tutorial::NetworkModule*>(handle);
    if (!core) return NETWORK_ERR_INVALID_HANDLE;
    
    try {
        core->stopHeartbeat();
        return NETWORK_SUCCESS;
    } catch (...) {
        return NETWORK_ERR_HEARTBEAT_FAILED;
    }
}