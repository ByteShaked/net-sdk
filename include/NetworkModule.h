// NetworkModule.h
#pragma once
#include "NetworkInterface.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <arpa/inet.h>
#include <unistd.h>
#include <tuple>
#include <string>
#include <algorithm>
#include "proto_x86_v19/msg2lite.pb.h"
#include "TlvHandler.h"
#include <map>
#include <chrono>
#include <pthread.h>
#include "NetLog.h"
struct PendingMessage {
    std::chrono::steady_clock::time_point send_time;
    std::shared_ptr<std::condition_variable_any> cv;
    bool timeout = false;
};
namespace tutorial {
    class NetworkModule : public NetworkInterface {
    public:
        NetworkModule(){};
        static NetworkModule* instance_;
        static std::mutex mutex_;
        ~NetworkModule() override;

        bool connect(const ConnectOptions& options) override;
        void sendMessage(ProtocolType type,  Person &MsgLiteInner) override;
        void setDataCallback(DataCallback callback) override;
        void setErrorCallback(ErrorCallback callback) override;
        void setNetStateCallback(NetStateCallback callback) override;
        void disconnect() override;
        static NetworkModule* getInstance() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance_ == nullptr) {
                instance_ = new NetworkModule();
            }
            return instance_;
        }
        void destroy() override;
    private:

        int sockfd_ = -1;
        std::atomic<bool> is_connected_{false};
        std::atomic<bool> stop_threads_{false};

        std::atomic<bool> heartbeat_enabled_{false};
        std::atomic<int> heartbeat_interval_sec_{20};

        bool disconnected_ = false; 
        
        tlv::TlvHandler tlvhandler_;
        // 回调函数
        DataCallback data_callback_;
        ErrorCallback error_callback_;
        NetStateCallback net_state_callback_; // 新增网络状态回调
        std::mutex callback_mutex_;

        // 线程管理
        std::thread receive_thread_;
        std::thread heartbeat_thread_;
        std::atomic<bool> threads_joined_{false}; // 新增原子标志
        std::thread connect_thread_;  // 新增连接线程
        std::thread reconnect_thread_; // 新增重连线程
        
        // 心跳包发送
        void sendHeartbeat();
        
        // 数据接收循环
        void receiveLoop();

        void notify_error(int code, const std::string& msg);
        void notify_state(ConnectionState state, const std::string& msg);
        bool try_connect();
        bool reconnect();
        // 内部线程函数
       // void reconnectThread(); // 新增重连线程函数        
        void stopWorkerThreads(); // 重命名为更准确的名称
        void startWorkerThreads(); // 新增工作线程启动函数

        std::atomic<uint32_t> proto_client_no_{0};  // 编号生成器
        // std::map<uint32_t, std::chrono::steady_clock::time_point> pending_messages_; // 待确认消息
        // std::unordered_map<uint32_t, PendingMessage> pending_messages_;
        std::unordered_set<uint32_t> pending_messages_;
        std::mutex pending_mutex_;                  // 保护映射
        std::atomic<bool> stop_timeout_check{false};    
        std::mutex socket_mutex_;

        std::condition_variable heartbeat_cv_; 
        std::mutex heartbeat_mutex_;

        std::mutex cv_mutex_;
        std::condition_variable response_cv_;

        // 重连相关成员
        int max_retry_count_ = 3;          // 最大重连次数
        int retry_interval_sec_ = 2;        // 重连间隔(秒)
        int current_retry_count_ = 0;      // 当前重连次数
        bool auto_reconnect_ = false;      // 是否自动重连
        std::string last_host_;             // 上次连接的主机
        int last_port_ = 0;                 // 上次连接的端口
        std::atomic<bool> reconnecting_{false};

        std::atomic<bool> receive_thread_running_{false};
        // 状态管理
        ConnectionState connection_state_ = ConnectionState::DISCONNECTED;
        mutable std::mutex state_mutex_;
        std::atomic<bool> is_destroying_{false};

        std::thread reconnect_manager_thread_;
        std::atomic<bool> reconnect_manager_running_{false};
        std::atomic<bool> reconnect_requested_{false};
        std::mutex reconnect_mutex_;
        std::condition_variable reconnect_cv_;

    public:
        void startHeartbeat();  // 启动心跳

        void stopHeartbeat();   // 停止心跳

        void setHeartbeatInterval(int seconds); // 设置间隔

        bool sendHeartbeatOnce();


        void stopReconnectManager();
        void startReconnectManager();
    };
    static NetworkModule* instance_;
    static std::mutex mutex_;
}