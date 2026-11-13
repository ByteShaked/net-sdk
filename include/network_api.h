#ifndef NETWORK_API_H
#define NETWORK_API_H

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
typedef enum {
    NETWORK_SUCCESS = 0,
    NETWORK_ERR_INVALID_HANDLE = -1,
    NETWORK_ERR_INVALID_PARAM = -2,
    NETWORK_ERR_CONNECT_FAILED = -3,
    NETWORK_ERR_SEND_EXCEPTION = -4,
    NETWORK_ERR_DNS_RESOLUTION_FAILED = -5,
    NETWORK_ERR_CONNECTION_FAILED = -6
} NetworkErrorCode;

// 心跳错误码
typedef enum {
    HEARTBEAT_OK = 0,
    HEARTBEAT_ERR_NOT_RUNNING = -1,
    HEARTBEAT_ERR_INVALID_INTERVAL = -2
} HeartbeatErrorCode;

typedef struct ConnectOptionsC {
    const char* host;
    int port;
    int max_retry_count;
    int retry_interval_sec;
    int auto_reconnect;  // 0 = false, 非0 = true
    int heartbeat_interval_sec;
} ConnectOptionsC;

// 不透明句柄类型
typedef void* NetworkHandle;

// 回调函数类型
typedef void (*DataCallback)(const char* data, int len, void* userdata);
typedef void (*ErrorCallback)(int  code, const char* msg, void* userdata);

// 创建/销毁实例
NetworkHandle network_create();
void network_destroy(NetworkHandle handle);

// 连接管理
int network_connect(NetworkHandle handle, const ConnectOptionsC* options);
void network_disconnect(NetworkHandle handle);

// 数据发送
int network_send(NetworkHandle handle, 
                const char* data, 
                int len,
                int msg_type,
                int encrypt);

// 回调设置
void network_set_callbacks(NetworkHandle handle,
                          DataCallback data_cb,
                          ErrorCallback error_cb,
                          void* userdata);

// 新增心跳控制接口
// --------------------------
int network_start_heartbeat(NetworkHandle handle, int interval_sec);
int network_stop_heartbeat(NetworkHandle handle);
// int network_set_heartbeat_interval(NetworkHandle handle, int interval_sec);   

#ifdef __cplusplus
}
#endif

#endif // NETWORK_API_H