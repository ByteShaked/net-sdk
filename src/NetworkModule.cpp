#include "NetworkModule.h"
#include <arpa/inet.h>
#include <unistd.h>
#include "proto_x86_v19/msg2lite.pb.h"
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "common.h"
#include <hilog/log.h>


#undef LOG_DOMAIN 
#undef LOG_TAG 
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域 
#define LOG_TAG "MY_TAG"  // 全局tag宏，标识模块日志tag

namespace tutorial {

    NetworkModule::~NetworkModule() {
        OH_LOG_INFO(LOG_APP, "~NetworkModule() called, disconnecting...");
        destroy();
    }
    void NetworkModule::startReconnectManager() {
        reconnect_manager_running_ = true;
        reconnect_manager_thread_ = std::thread([this] {
            while (reconnect_manager_running_) {
                std::unique_lock<std::mutex> lk(reconnect_mutex_);
                reconnect_cv_.wait(lk, [this] { return reconnect_requested_ || !reconnect_manager_running_; });
                if (!reconnect_manager_running_) break;
                reconnect_requested_ = false;
                this->reconnect(); // 只在管理线程里调用
            }
        });
    }

    void NetworkModule::stopReconnectManager() {
        reconnect_manager_running_ = false;
        reconnect_cv_.notify_all();
        if (reconnect_manager_thread_.joinable()) {
            reconnect_manager_thread_.join();
        }
    }

    bool NetworkModule::connect(const ConnectOptions& options) {
       
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (connection_state_ != ConnectionState::DISCONNECTED) {
            OH_LOG_WARN(LOG_APP, "Connect called in invalid state: %d", static_cast<int>(connection_state_));
            return false;
        }       
        // 保存连接参数
        last_host_ = options.host;
        last_port_ = options.port;
        max_retry_count_ = options.max_retry_count;
        retry_interval_sec_ = options.retry_interval_sec;
        current_retry_count_ = 0;
        auto_reconnect_ = options.auto_reconnect;
        setHeartbeatInterval(options.heartbeat_interval_sec);
        OH_LOG_INFO(LOG_APP, "Connecting to [%{public}s]:[%{public}d] with [%{public}d] retries ([%{public}d] s interval), heartbeat [%{public}d]s",
                    last_host_.c_str(), last_port_, max_retry_count_, retry_interval_sec_, options.heartbeat_interval_sec);
        connection_state_ = ConnectionState::CONNECTING;
        try_connect();  
        return true;
    }
    bool NetworkModule::try_connect() {
        try {
           printf("ip: [%s] port:[%d]\n",last_host_.c_str(), last_port_);
           OH_LOG_INFO(LOG_APP, "Net host:  [%{public}s]",last_host_.c_str());
           OH_LOG_INFO(LOG_APP, "Net port:  [%{public}d]", last_port_);
        //    network_log::Logger::Write(network_log::LogLevel::INFO, "ip: [%s] port:[%d]\n",host.c_str(), port);
            
            // 解析域名
            boost::asio::io_service io_service;
            boost::system::error_code ec;
            boost::asio::ip::tcp::resolver resolver(io_service);
            boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(),last_host_, std::to_string(last_port_));
            auto endpoints = resolver.resolve(query,ec);
            if (ec) {
                notify_error(NETWORK_ERR_DNS_RESOLUTION_FAILED, "DNS解析失败: " + ec.message());
                return false;
            }
                
            // 创建socket
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                notify_error(NETWORK_ERR_SOCKET_CREATION_FAILED, "Socket creation failed");
                return false;
            }
            int flag = 1;
            if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int))) {
                notify_error(NETWORK_ERR_SOCKET_OPTION_FAILED, "Set socket option failed");
                close(sock);
                return false;
            }

            // 设置连接超时
            struct timeval timeout;
            timeout.tv_sec = 5;  // 5秒连接超时
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            // 尝试连接所有解析到的地址
            auto endpoint = *endpoints;
            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(last_port_);

            if (endpoint.endpoint().address().is_v4()) {
                serv_addr.sin_addr.s_addr = 
                    boost::asio::detail::socket_ops::host_to_network_long(
                        endpoint.endpoint().address().to_v4().to_ulong());
                        auto addr = endpoint.endpoint().address();
                        OH_LOG_INFO(LOG_APP, "Trying IP: [%{public}s]", 
                                    addr.to_string().c_str());
                        // printf("connect ip is : %s \n",addr.c_str());
                if (::connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
                    OH_LOG_INFO(LOG_APP, "Connected to IP: [%{public}s]", 
                                addr.to_string().c_str());
                    last_host_ = addr.to_string();
                    // 更新socket
                    {
                        std::lock_guard<std::mutex> lock(socket_mutex_);
                        if (sockfd_ != -1) close(sockfd_);
                        sockfd_ = sock;
                    }
                }
                else
                {
                    OH_LOG_INFO(LOG_APP, "Failed to connect to IP: [%{public}s]", 
                                addr.to_string().c_str());
                    notify_error(NETWORK_ERR_CONNECTION_FAILED, "Connection to all resolved addresses failed");
                    close(sock);
                    return false;
                }
    
            }
            // OH_LOG_INFO(LOG_APP, "connection_state_: [%{public}d]", 
            //                    connection_state_);
            // // 更新连接状态
            {
                // std::lock_guard<std::mutex> lock(state_mutex_);    
                connection_state_ = ConnectionState::CONNECTED;
                stop_threads_ = false; 
            }
            // 启动工作线程
            if (receive_thread_running_) {
                OH_LOG_WARN(LOG_APP, "Receive thread already running, skip startWorkerThreads");
            } else {
                startWorkerThreads();
            }
            //通知上层网络状态
            notify_error(NETWORK_SUCCESS, "Connected successfully");

            return true;

        } catch (const boost::system::system_error& e) {
            notify_error(NETWORK_ERR_DNS_RESOLUTION_FAILED, std::string("DNS resolution failed: ") + e.what());
            return false;
        } catch (...) {
            notify_error(NETWORK_ERR_DNS_UNKNOWN, "Unknown error during DNS resolution");
            return false;
        }
    }
    void NetworkModule::startWorkerThreads() {
        // 重置停止标志
        stop_threads_ = false;
    // 避免重复启动线程
    if (receive_thread_.joinable()) {
        if (receive_thread_.get_id() != std::this_thread::get_id()) {
            receive_thread_.join();
        } else {
            receive_thread_.detach();
        }
    }
        // 启动接收线程
        receive_thread_ = std::thread(&NetworkModule::receiveLoop, this);
        receive_thread_running_ = true;
        // 启动心跳线程
        startHeartbeat();
    }
    void NetworkModule::stopWorkerThreads() {
        if (stop_threads_) return;
        // 设置停止标志
        stop_threads_ = true;
          // 清空待确认消息并唤醒等待线程
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_messages_.clear();
        }
        // 中断socket操作
        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (sockfd_ != -1) {
                shutdown(sockfd_, SHUT_RDWR);
            }
        }
        
        // 等待接收线程结束
        if (receive_thread_.joinable() && receive_thread_.get_id() != std::this_thread::get_id()) {
            receive_thread_.join();
        }
        //  else if (receive_thread_.joinable()) {
        //     receive_thread_.detach();
        // }
        receive_thread_running_ = false;
        
        // 停止心跳线程
        stopHeartbeat();
        if (heartbeat_thread_.joinable() && heartbeat_thread_.get_id() != std::this_thread::get_id()) {
            heartbeat_thread_.join();
        } 
        // else if (heartbeat_thread_.joinable()) {
        //     heartbeat_thread_.detach();
        // }        
        // 关闭socket
        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (sockfd_ != -1) {
                close(sockfd_);
                sockfd_ = -1;
            }
        }
    }

    bool NetworkModule::reconnect() {
        if (is_destroying_) return false;
        {
           
            std::lock_guard<std::mutex> lock(state_mutex_);
            
            if (connection_state_ == ConnectionState::RECONNECTING) 
            {
                OH_LOG_INFO(LOG_APP, "Reconnect skipped, invalid state: [%{public}d]", 
                        static_cast<int>(connection_state_));
                return false;
            }
            
            OH_LOG_INFO(LOG_APP, "Starting reconnect to [%{public}s]:[%{public}d]", 
                    last_host_.c_str(), last_port_);
            
            // 更新状态
            connection_state_ = ConnectionState::RECONNECTING;
        }
        // 停止当前工作线程
        stopWorkerThreads();

        current_retry_count_ = 0;

        // 启动重连线程
        // reconnect_thread_ = std::thread([this] {
            while (current_retry_count_ <= max_retry_count_) {
                if (is_destroying_) return false;
                // 检查是否需要继续重连
                { 
                    OH_LOG_INFO(LOG_APP, "Reconnect start");
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (connection_state_ != ConnectionState::RECONNECTING) {
                        OH_LOG_INFO(LOG_APP, "Reconnect aborted. State changed to: [%{public}d]", 
                                static_cast<int>(connection_state_));
                        return false; // 状态改变时退出循环
                    }
                }                
                OH_LOG_INFO(LOG_APP, "Reconnect attempt [%{public}d][%{public}d]", current_retry_count_ + 1, max_retry_count_);
                
                if (try_connect()) {
                    OH_LOG_INFO(LOG_APP, "Reconnect successful");
                    return true;
                }
            }
            {
                // 线程结束时重置状态
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (connection_state_ == ConnectionState::RECONNECTING) {
                    connection_state_ = ConnectionState::DISCONNECTED;
                notify_error(NETWORK_ERR_RECONNECT_FAILED, "Reconnect failed after " + 
                            std::to_string(current_retry_count_) + " attempts");
                }
            }
            return false;
        // });        
        return true;
    }

    void NetworkModule::sendMessage(ProtocolType type, Person &MsgLiteInner) {
        if (stop_threads_ ) 
        {
            notify_error(NETWORK_ERR_CONNECTION_FAILED, "Connection is stopped");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (connection_state_ != ConnectionState::CONNECTED) {
                notify_error(NETWORK_ERR_CONNECTION_FAILED, "Not connected");
                return;
            }
        }
        // 生成唯一编号
        uint32_t current_no = proto_client_no_.fetch_add(1);
        MsgLiteInner.set_protoclientno(current_no);  // 假设Person类有protoclientno字段
        // 添加到待确认列表
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_messages_.insert(current_no);       
        }
        std::cout <<"send protoclientno : "<< current_no <<std::endl;
        // 序列化到字符串
        std::string serialized_str;
        MsgLiteInner.SerializeToString(&serialized_str);
        // std::vector<uint8_t> data(serialized_str.begin(), serialized_str.end());

        tlv::Packet packet = tlv::TlvHandler::encode(MsgLiteInner);

        std::vector<uint8_t> raw_data(packet.body.begin(), packet.body.end());
        // raw_data.push_back(packet.magic >> 8);
        // raw_data.push_back(packet.magic & 0xFF);
        // raw_data.insert(raw_data.end(), packet.body.begin(), packet.body.end());
        // raw_data.push_back(packet.crc >> 8);
        // raw_data.push_back(packet.crc & 0xFF);
        // if (!heartbeat_enabled_ || !is_connected_ || stop_threads_) {
        //     notify_error(NETWORK_ERR_CONNECTION_FAILED, "send message start connection is closed or stopped");
        //     return;
        // }


        // 发送数据
        bool send_ok = false;
        {
            // 发送前再次检查socket有效性
            std::lock_guard<std::mutex> sock_lock(socket_mutex_);
            if (sockfd_ == -1) {
                notify_error(NETWORK_ERR_SOCKET_INVALID, "Invalid socket in heartbeat");
                return;
            }
            // OH_LOG_INFO(LOG_APP, "raw_data:  [%{public}d]",raw_data.size());
            send_ok = (send(sockfd_, raw_data.data(), raw_data.size(), 0) >= 0);
        }

        if (!send_ok) {
            notify_error(NETWORK_ERR_SEND_FAILED, "Send failed: " + std::to_string(current_no));
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_messages_.erase(current_no);
            return;
        }

        // 同步等待响应或超时
        bool confirmed = false;
        {
            std::unique_lock<std::mutex> cv_lock(cv_mutex_);
            confirmed = response_cv_.wait_for(cv_lock, std::chrono::seconds(10), [this, current_no] {
                // if (stop_threads_ || disconnected_) {
                //  OH_LOG_INFO(LOG_APP, "wait_for:  [%{public}s]","connection is closed or stopped");
                //     return true; // 立即唤醒
                // }
                return pending_messages_.find(current_no) == pending_messages_.end();
            });
        }
        if (!confirmed) {
            // 超时处理
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_messages_.erase(current_no);
            notify_error(NETWORK_ERR_TIMEOUT, "Timeout for message: " + std::to_string(current_no));
        }
    }
    void NetworkModule::setDataCallback(DataCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        data_callback_ = std::move(callback);
    }
    void NetworkModule::setErrorCallback(ErrorCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        error_callback_ = std::move(callback);
    }
    void NetworkModule::setNetStateCallback(NetStateCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        net_state_callback_ = std::move(callback);
    }

    void NetworkModule::disconnect() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_); 
            if (connection_state_ == ConnectionState::DISCONNECTED) {
                return;
            }
            
            OH_LOG_INFO(LOG_APP, "Disconnecting... [%{public}d]", 
                        static_cast<int>(connection_state_));
            
            // 更新状态
            connection_state_ = ConnectionState::DISCONNECTED;
            stop_threads_ = true;
            disconnected_ = true;
        }
        // 唤醒所有等待的线程
        response_cv_.notify_all();
        
        // 停止所有工作线程
        stopWorkerThreads();
        // 停止连接线程
        if (connect_thread_.joinable()) {
            if (connect_thread_.get_id() != std::this_thread::get_id()) {
                connect_thread_.join();
            } else {
                connect_thread_.detach();
            }
        }
        stopReconnectManager(); // 停止重连管理线程
        // 停止重连线程
        if (reconnect_thread_.joinable()) {
            if (reconnect_thread_.get_id() != std::this_thread::get_id()) {
                reconnect_thread_.join();
            } else {
                reconnect_thread_.detach();
            }
        }
        

    }
    void NetworkModule::destroy() {
        is_destroying_ = true;
        disconnect();
         // 清空回调
        {
            std::lock_guard<std::mutex> cb_lock(callback_mutex_);
            data_callback_ = nullptr;
            error_callback_ = nullptr;
            net_state_callback_ = nullptr;
        }
        // 清空待确认队列
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_messages_.clear();
        }
        
    }
    // 新增：单次心跳发送（同步）
    bool NetworkModule::sendHeartbeatOnce() {
        const std::vector<uint8_t> heartbeat{0x00, 0x00};
        std::lock_guard<std::mutex> lock(socket_mutex_);
        if (sockfd_ == -1) return false;

        int ret = send(sockfd_, heartbeat.data(), heartbeat.size(), 0);
        if (ret < 0) {
            OH_LOG_INFO(LOG_APP, "Heartbeat failed after reconnect");
            notify_error(NETWORK_ERR_HEARTBEAT_FAILED, "Heartbeat failed after reconnect");
            return false;
        }
        return true;
    }
    void NetworkModule::startHeartbeat() {  // 启动心跳
        std::lock_guard<std::mutex> lock(heartbeat_mutex_);
        // 确保没有旧的心跳线程在运行
        if (heartbeat_thread_.joinable()) {
            heartbeat_enabled_ = false;
            heartbeat_thread_.join();
        }
        if (connection_state_ == ConnectionState::CONNECTED)
        {
            heartbeat_enabled_ = true;
            heartbeat_thread_ = std::thread(&NetworkModule::sendHeartbeat, this);
        }
    }
    void NetworkModule::stopHeartbeat() {   // 停止心跳
        // std::lock_guard<std::mutex> lock(heartbeat_mutex_);
        heartbeat_enabled_ = false;
        // if (heartbeat_thread_.joinable()) {
        //     // 等待线程结束
        //     heartbeat_thread_.join();
        //     OH_LOG_INFO(LOG_APP, "Heartbeat thread stopped");
        // }
    }
    void NetworkModule::setHeartbeatInterval(int seconds) {  // 设置心跳间隔
        if (seconds > 0) heartbeat_interval_sec_ = seconds;
    }    
    void NetworkModule::sendHeartbeat() {
        const std::vector<uint8_t> heartbeat{0x00, 0x00};
        OH_LOG_INFO(LOG_APP, "Heartbeat thread join start !");
        while (heartbeat_enabled_ && !stop_threads_  && !is_destroying_) {
            std::this_thread::sleep_for(std::chrono::seconds(heartbeat_interval_sec_));
            if (stop_threads_) break;
        
            // 发送前再次检查socket有效性
            std::lock_guard<std::mutex> sock_lock(socket_mutex_);
            if (sockfd_ == -1) {
                notify_error(NETWORK_ERR_SOCKET_INVALID, "Invalid socket in heartbeat");
                break;
            }
            std::lock_guard<std::mutex> lock(heartbeat_mutex_);
            if (send(sockfd_, heartbeat.data(), heartbeat.size(), 0) < 0) {
                notify_error(NETWORK_ERR_HEARTBEAT_FAILED, "Heartbeat failed");
                break;
            }
        }
    }
    void NetworkModule::receiveLoop() {
        std::vector<uint8_t> buffer;
        OH_LOG_INFO(LOG_APP, "receiveLoop thread join start !");
        while (!stop_threads_ && !is_destroying_) {
            // 检查连接状态
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (connection_state_ == ConnectionState::RECONNECTING) {
                    OH_LOG_INFO(LOG_APP, "Receive thread exiting, not connected");
                    break;
                }
            }
            // 检查socket是否有效
            if (sockfd_ == -1) {
                // 等待重连完成
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            uint8_t temp[1024];
            size_t bytes = recv(sockfd_, temp, sizeof(temp), 0);
            if (bytes == 0) {
                OH_LOG_INFO(LOG_APP, "Connection closed by peer");
                bool need_reconnect = false;
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (auto_reconnect_ && connection_state_ == ConnectionState::CONNECTED) {
                        need_reconnect = true;
                    }
                }
                if (need_reconnect) {
                    // 异步触发重连
                    {
                        std::lock_guard<std::mutex> lk(reconnect_mutex_);
                        reconnect_requested_ = true;
                    }
                    reconnect_cv_.notify_one();
                    break;
                } else {
                    notify_error(NETWORK_ERR_CONNECTION_CLOSED, "Connection closed by peer");
                    disconnect();
                    break;
                }
            } else if (bytes < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                OH_LOG_ERROR(LOG_APP, "Receive error: %d", errno);
                bool need_reconnect = false;
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (auto_reconnect_ && connection_state_ == ConnectionState::CONNECTED) {
                        need_reconnect = true;
                    }
                }
                if (need_reconnect) {
                    // 异步触发重连
                    {
                        std::lock_guard<std::mutex> lk(reconnect_mutex_);
                        reconnect_requested_ = true;
                    }
                    reconnect_cv_.notify_one();
                }
                 break;
            }
            if (bytes == 2 && temp[0] == 0x00 && temp[1] == 0x00) {
                OH_LOG_INFO(LOG_APP, "Net Received heartbeat packet, ignore.");
                break;
            }
            // 将数据加入缓冲区
            buffer.insert(buffer.end(), temp, temp);
            // std::vector<uint8_t> buffer(temp, temp + bytes);
            
            while (true)
            { // 循环处理多个消息
                // 解析完整数据帧
                auto proto_data = tlvhandler_.parse_tlv_packet(buffer);
                auto  parse_msg_size =  std::get <0> (proto_data);
                auto  callback_msg =  std::get <1> (proto_data);
                std::string error_msg = std::get<2>(proto_data);
                if (!error_msg.empty()) {
                    int tlv_error_code = 0;
                    //暂时去除包头包尾格式校验报错回调
                    // notify_error(tlv_error_code, error_msg);
                    OH_LOG_INFO(LOG_APP, "Net tlv_error_code:  [%{public}s]",error_msg.c_str());
                }
                {
                    tutorial::Person person;
                    const std::string serialized_str(callback_msg.begin(), callback_msg.end());
                    if (person.ParseFromString(serialized_str)) {
                        uint32_t received_no = person.protoclientno();
                        std::cout << "protoclientno: " << person.protoclientno() << std::endl;
                        OH_LOG_INFO(LOG_APP, "Net  received_no:  [%{public}d][%{public}zu]",received_no,pending_messages_.size());
                        OH_LOG_INFO(LOG_APP, "Net  prototype:  [%{public}d]",person.prototype());                        
                        bool is_pending = false;
                        // 检查protoclientno是否在pending_messages_中
                        //（属于服务器下发通知处理（如果received_no在clinetno（pending_messages_）中没找到，或者received_no为0））
                        //  属于服务器响应消息通知处理，移除pending_messages_中的条目并通知
                        {
                            std::lock_guard<std::mutex> lock(pending_mutex_);
                            is_pending = (pending_messages_.erase(received_no) > 0);
                        }
                        if (is_pending) {
                            // 是请求响应，通知等待线程
                            std::unique_lock<std::mutex> cv_lock(cv_mutex_);
                            response_cv_.notify_all();

                            std::lock_guard<std::mutex> lock(callback_mutex_);
                            if (data_callback_ && !callback_msg.empty())
                                {
                                    data_callback_(callback_msg);
                                } 
                            }else{
                                std::lock_guard<std::mutex> lock(callback_mutex_);
                                if (data_callback_ && !callback_msg.empty())
                                    {
                                        data_callback_(callback_msg);
                                    }
                            } 
                        }
                }                                                                                                                                                              
                continue;
            }
            // buffer.clear();
        }       
        // stop_threads_ = true;
    }
    void NetworkModule::notify_error(int code, const std::string& msg) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        try {
            if (error_callback_ ) { // 新增状态检查
                error_callback_(code, msg);
            }
        }catch (const std::exception& e) {
            OH_LOG_ERROR(LOG_APP, "Error in notify_error: %s", e.what());
        } catch (...) {
            OH_LOG_ERROR(LOG_APP, "Unknown error in notify_error");
        }
    }
    void NetworkModule::notify_state(ConnectionState state, const std::string& msg) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (net_state_callback_) {
            net_state_callback_(state, msg);
        }
    }
}