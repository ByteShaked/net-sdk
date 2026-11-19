#include "NetworkModule.h"
#include <iostream>

// 根据编译目标选择不同的网络库
#if defined(__OHOS__) || (defined(__linux__) && defined(__aarch64__))
    // 鸿蒙或 Linux ARM64：使用原生 POSIX API
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netinet/tcp.h>
    #define USE_NATIVE_SOCKET 1
#else
    // 其他平台：使用 Boost.Asio
    #include <boost/asio.hpp>
    #include <boost/asio/ip/tcp.hpp>
    #define USE_NATIVE_SOCKET 0

    // Windows 平台定义
    #ifdef _WIN32
        #define SHUT_RDWR SD_BOTH
        #ifndef _WIN32_WINNT
            #define _WIN32_WINNT 0x0601  // Windows 7
        #endif
    #endif
#endif

#include "common.h"
#include "net_log_macro.h"

namespace tutorial {

    NetworkModule::~NetworkModule() {
        NET_LOG_INFO("~NetworkModule() called, disconnecting...");
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
            NET_LOG_WARN("Connect called in invalid state: %d", static_cast<int>(connection_state_));
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
        NET_LOG_INFO("Connecting to [%s]:[%d] with [%d] retries ([%d] s interval), heartbeat [%d]s",
                    last_host_.c_str(), last_port_, max_retry_count_, retry_interval_sec_, options.heartbeat_interval_sec);
        connection_state_ = ConnectionState::CONNECTING;
        try_connect();
        return true;
    }
    bool NetworkModule::try_connect() {
        try {
           printf("ip: [%s] port:[%d]\n",last_host_.c_str(), last_port_);
           NET_LOG_INFO("Net host:  [%s]",last_host_.c_str());
           NET_LOG_INFO("Net port:  [%d]", last_port_);

#if USE_NATIVE_SOCKET
            // ===== 鸿蒙/Linux ARM64：使用原生 POSIX getaddrinfo() =====
            struct addrinfo hints, *res, *p;
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC;     // IPv4 或 IPv6
            hints.ai_socktype = SOCK_STREAM; // TCP

            // DNS 解析
            int status = getaddrinfo(last_host_.c_str(), std::to_string(last_port_).c_str(), &hints, &res);
            if (status != 0) {
                notify_error(NETWORK_ERR_DNS_RESOLUTION_FAILED, "DNS解析失败: " + std::string(gai_strerror(status)));
                return false;
            }

            // 尝试连接所有解析到的地址
            int sockfd = -1;
            for (p = res; p != NULL; p = p->ai_next) {
                sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (sockfd == -1) continue;

                if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
                    break; // 连接成功
                }
                close(sockfd);
                sockfd = -1;
            }

            freeaddrinfo(res);

            if (sockfd == -1) {
                notify_error(NETWORK_ERR_CONNECTION_FAILED, "Connection failed");
                return false;
            }

            NET_LOG_INFO("Connected to [%s]:[%d]", last_host_.c_str(), last_port_);

            // 设置 TCP_NODELAY（禁用 Nagle 算法，降低延迟）
            int flag = 1;
            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

            // 保存 socket fd
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                if (sockfd_ != -1) {
                    close(sockfd_);
                }
                sockfd_ = sockfd;
            }

#else
            // ===== 其他平台：使用 Boost.Asio =====
            boost::asio::io_context io_ctx;
            boost::system::error_code ec;
            boost::asio::ip::tcp::resolver resolver(io_ctx);
            auto endpoints = resolver.resolve(last_host_, std::to_string(last_port_), ec);
            if (ec) {
                notify_error(NETWORK_ERR_DNS_RESOLUTION_FAILED, "DNS解析失败: " + ec.message());
                return false;
            }

            // 创建 Boost socket，尝试连接所有解析到的地址
            boost::asio::ip::tcp::socket boost_sock(io_ctx);
            boost::asio::connect(boost_sock, endpoints, ec);
            if (ec) {
                NET_LOG_INFO("Failed to connect: [%s]", ec.message().c_str());
                notify_error(NETWORK_ERR_CONNECTION_FAILED, "Connection failed: " + ec.message());
                return false;
            }

            NET_LOG_INFO("Connected to [%s]:[%d]", last_host_.c_str(), last_port_);

            // 设置 TCP_NODELAY（禁用 Nagle 算法，降低延迟）
            boost_sock.set_option(boost::asio::ip::tcp::no_delay(true), ec);

            // 取出原生 fd，后续收发仍用原生 socket API（跨平台）
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                if (sockfd_ != -1) {
#ifdef _WIN32
                    closesocket(sockfd_);
#else
                    close(sockfd_);
#endif
                }
                sockfd_ = (int)boost_sock.release();
            }
#endif

            // 更新连接状态
            {
                connection_state_ = ConnectionState::CONNECTED;
                stop_threads_ = false;
            }
            // 启动工作线程
            if (receive_thread_running_) {
                NET_LOG_WARN("Receive thread already running, skip startWorkerThreads");
            } else {
                startWorkerThreads();
            }
            //通知上层网络状态
            notify_error(NETWORK_SUCCESS, "Connected successfully");

            return true;

        } catch (const std::exception& e) {
            notify_error(NETWORK_ERR_DNS_RESOLUTION_FAILED, std::string("Connection failed: ") + e.what());
            return false;
        } catch (...) {
            notify_error(NETWORK_ERR_DNS_UNKNOWN, "Unknown error during connection");
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
            NET_LOG_INFO("Joining receive thread...");
            receive_thread_.join();
            NET_LOG_INFO("Receive thread joined");
        }
        receive_thread_running_ = false;

        // 停止心跳线程
        heartbeat_enabled_ = false;
        if (heartbeat_thread_.joinable() && heartbeat_thread_.get_id() != std::this_thread::get_id()) {
            NET_LOG_INFO("Joining heartbeat thread...");
            heartbeat_thread_.join();
            NET_LOG_INFO("Heartbeat thread joined");
        }
        // 关闭socket
        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (sockfd_ != -1) {
#ifdef _WIN32
                closesocket(sockfd_);
#else
                close(sockfd_);
#endif
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
                NET_LOG_INFO("Reconnect skipped, invalid state: [%d]", 
                        static_cast<int>(connection_state_));
                return false;
            }
            
            NET_LOG_INFO("Starting reconnect to [%s]:[%d]", 
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
                    NET_LOG_INFO("Reconnect start");
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (connection_state_ != ConnectionState::RECONNECTING) {
                        NET_LOG_INFO("Reconnect aborted. State changed to: [%d]", 
                                static_cast<int>(connection_state_));
                        return false; // 状态改变时退出循环
                    }
                }                
                NET_LOG_INFO("Reconnect attempt [%d][%d]", current_retry_count_ + 1, max_retry_count_);
                
                if (try_connect()) {
                    NET_LOG_INFO("Reconnect successful");
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

            // 调试：打印发送的前32字节原始数据
            printf("[DEBUG] Sending %zu bytes, first 32 bytes (hex): ", raw_data.size());
            for (size_t i = 0; i < std::min(raw_data.size(), size_t(32)); i++) {
                printf("%02x ", raw_data[i]);
            }
            printf("\n");

            // NET_LOG_INFO("raw_data:  [%d]",raw_data.size());
            send_ok = (send(sockfd_, reinterpret_cast<const char*>(raw_data.data()), static_cast<int>(raw_data.size()), 0) >= 0);
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
                //  NET_LOG_INFO("wait_for:  [%s]","connection is closed or stopped");
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
            
            NET_LOG_INFO("Disconnecting... [%d]", 
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

        int ret = send(sockfd_, reinterpret_cast<const char*>(heartbeat.data()), static_cast<int>(heartbeat.size()), 0);
        if (ret < 0) {
            NET_LOG_INFO("Heartbeat failed after reconnect");
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
        //     NET_LOG_INFO("Heartbeat thread stopped");
        // }
    }
    void NetworkModule::setHeartbeatInterval(int seconds) {  // 设置心跳间隔
        if (seconds > 0) heartbeat_interval_sec_ = seconds;
    }    
    void NetworkModule::sendHeartbeat() {
        const std::vector<uint8_t> heartbeat{0x00, 0x00};
        NET_LOG_INFO("Heartbeat thread join start !");
        while (heartbeat_enabled_ && !stop_threads_  && !is_destroying_) {
            std::this_thread::sleep_for(std::chrono::seconds(heartbeat_interval_sec_));
            if (stop_threads_) break;

            NET_LOG_INFO("Heartbeat: attempting to send...");  // ✅ 添加调试日志

            // 发送前再次检查socket有效性
            std::lock_guard<std::mutex> sock_lock(socket_mutex_);
            if (sockfd_ == -1) {
                notify_error(NETWORK_ERR_SOCKET_INVALID, "Invalid socket in heartbeat");
                break;
            }
            std::lock_guard<std::mutex> lock(heartbeat_mutex_);
            int ret = send(sockfd_, reinterpret_cast<const char*>(heartbeat.data()), static_cast<int>(heartbeat.size()), 0);
            if (ret < 0) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "Heartbeat failed: errno=%d (%s)",
                        errno, strerror(errno));
                NET_LOG_INFO("%s", err_msg);
                notify_error(NETWORK_ERR_HEARTBEAT_FAILED, err_msg);
                break;
            } else {
                NET_LOG_INFO("Heartbeat sent successfully, %d bytes", ret);
            }
        }
    }
    void NetworkModule::receiveLoop() {
        std::vector<uint8_t> buffer;
        NET_LOG_INFO("receiveLoop thread join start !");
        while (!stop_threads_ && !is_destroying_) {
            // 检查连接状态
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (connection_state_ == ConnectionState::RECONNECTING) {
                    NET_LOG_INFO("Receive thread exiting, not connected");
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
            int bytes = recv(sockfd_, reinterpret_cast<char*>(temp), sizeof(temp), 0);
            if (bytes == 0) {
                NET_LOG_INFO("Connection closed by peer");
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
                NET_LOG_ERROR("Receive error: %d", errno);
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
                NET_LOG_INFO("Net Received heartbeat packet, ignore.");
                continue;  // ✅ 改为 continue，继续接收下一个数据
            }
            // 将数据加入缓冲区
            buffer.insert(buffer.end(), temp, temp + bytes);  // ✅ 修正：插入 bytes 字节
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
                    NET_LOG_INFO("Net tlv_error_code:  [%s]",error_msg.c_str());

                    // ✅ 解析失败，清空缓冲区避免死循环
                    if (parse_msg_size == 0) {
                        buffer.clear();
                        break;
                    }
                }
                {
                    tutorial::Person person;
                    const std::string serialized_str(callback_msg.begin(), callback_msg.end());
                    if (person.ParseFromString(serialized_str)) {
                        uint32_t received_no = person.protoclientno();
                        std::cout << "protoclientno: " << person.protoclientno() << std::endl;
                        NET_LOG_INFO("Net  received_no:  [%d][%zu]",received_no,pending_messages_.size());
                        NET_LOG_INFO("Net  prototype:  [%d]",person.prototype());                        
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

                // ✅ 解析成功，从缓冲区移除已处理的数据
                if (parse_msg_size > 0) {
                    buffer.erase(buffer.begin(), buffer.begin() + parse_msg_size);
                }

                // ✅ 如果缓冲区数据不足，跳出内层循环，继续接收数据
                if (buffer.empty() || buffer.size() < 2) {
                    break;
                }
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
            NET_LOG_ERROR("Error in notify_error: %s", e.what());
        } catch (...) {
            NET_LOG_ERROR("Unknown error in notify_error");
        }
    }
    void NetworkModule::notify_state(ConnectionState state, const std::string& msg) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (net_state_callback_) {
            net_state_callback_(state, msg);
        }
    }
}