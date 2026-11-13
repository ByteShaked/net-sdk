#pragma once
#include <functional>
#include <vector>
#include "TlvHandler.h"  // 包含 MsgLite.pb.h 和 Person 别名

namespace tutorial {
    enum class ProtocolType {
        FULL_AUTH = 0,
        TOKEN_AUTH = 1,
        IM_DATA = 2
    };
    // 连接状态枚举
    enum class ConnectionState {
        DISCONNECTED, //已断开
        CONNECTING,   //正在尝试连接
        CONNECTED,    //连接成功
        RECONNECTING  //正在尝试重连
    };

    struct ConnectOptions {
        std::string host;
        int port = 0;
        int max_retry_count = 30;
        int retry_interval_sec = 3;
        bool auto_reconnect = false;
        int heartbeat_interval_sec = 5; 
    };

    // 定义回调类型
    using DataCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using ErrorCallback = std::function<void(int,const std::string& error)>;
    using NetStateCallback = std::function<void(ConnectionState,const std::string& error)>;

    class NetworkInterface {
    public:
        using Callback = std::function<void(bool success, const std::string& error)>;
        
        virtual ~NetworkInterface() = default;
        
        // 初始化连接
        virtual bool connect(const ConnectOptions& options) = 0;
        // 发送消息接口
        virtual void sendMessage(ProtocolType type,   Person &MsgLiteInner) = 0;      
        // 注册数据回调
        virtual void setDataCallback(DataCallback callback) = 0;

        // 注册错误回调
        virtual void setErrorCallback(ErrorCallback callback) = 0;
        // 注册网络状态回调
        // 该回调用于通知连接状态变化，如连接成功、断开连接等
        virtual void setNetStateCallback(NetStateCallback callback) = 0;
        // 关闭连接
        virtual void disconnect() = 0;
        virtual void destroy() = 0;
    };
}