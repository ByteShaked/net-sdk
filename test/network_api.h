#ifndef NETWORK_API_H
#define NETWORK_API_H

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄类型
typedef void* NetworkHandle;

// 新增心跳错误码
typedef enum {
    HEARTBEAT_OK = 0,
    HEARTBEAT_ERR_NOT_RUNNING,
    HEARTBEAT_ERR_INVALID_INTERVAL
} HeartbeatErrorCode;
typedef enum {
    NETWORK_OK = 0,
    NETWORK_ERR_INVALID_HANDLE,
    NETWORK_ERR_CONNECTION_FAILED,
    NETWORK_ERR_SEND_FAILED,
    NETWORK_ERR_HEARTBEAT_FAILED
} NetworkErrorCode;

// 回调函数类型
typedef void (*DataCallback)(const char* data, int len, void* userdata);
typedef void (*ErrorCallback)(int  code, const char* msg, void* userdata);

// 创建/销毁实例
NetworkHandle network_create();
void network_destroy(NetworkHandle handle);

// 连接管理
int network_connect(NetworkHandle handle, const char* host, int port);
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