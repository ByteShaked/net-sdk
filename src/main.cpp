
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include "NetworkModule.h"
#include "proto_x86_v19/msg2lite.pb.h"
#include "proto_x86_v19/UserAuth.pb.h"
#include "proto_x86_v19/UserAuthResp.pb.h"
#include "proto_x86_v19/SendMsg.pb.h"
#include "TlvHandler.h"
#include <google/protobuf/util/json_util.h>

using namespace google::protobuf::util;

int main(int argc, char** argv) {

    tutorial::UserAuthInner UserAuthInner;
    UserAuthInner.set_appid("2466fc0c84220a57ecb7f0c9d6ad2460");
    UserAuthInner.set_username("zc6818518880782");
    UserAuthInner.set_authtype(1);
    UserAuthInner.set_timestamp("20250306135900");
    UserAuthInner.set_device(32);
    UserAuthInner.set_sig("1620E130DA9DDF8C75752CE789831436");
    UserAuthInner.set_sdkversion("8.0.1");
    UserAuthInner.set_imei("1234567890");
    UserAuthInner.set_mode(1);
    UserAuthInner.set_network(1);
    UserAuthInner.set_bundleid("1");
    UserAuthInner.set_subdevice("huawei");
    UserAuthInner.set_language("zh_CN");
    UserAuthInner.set_extopts("huawei");


    std::string inner_serialized_str;
    UserAuthInner.SerializeToString(&inner_serialized_str);
    // 创建一个 Person 对象
    tutorial::Person MsgLiteInner;
    MsgLiteInner.set_prototype(20);
    MsgLiteInner.set_protodata(inner_serialized_str);
    // MsgLiteInner.set_protoclientno(1234);
    // 初始化网络模块
    tutorial::NetworkModule network;

    // 注册回调
    network.setDataCallback([](const std::vector<uint8_t>& data) {
        std::cout << "Received data (length: " << data.size() << " bytes)" << std::endl;
        std::cout << "Received data : " << data.data() << std::endl;
        tutorial::Person person;
        const std::string serialized_str(data.begin(), data.end());
        person.ParseFromString(serialized_str);
        std::cout << "prototype: " << person.prototype() << std::endl;
        std::cout << "protoerrorcode: " << person.protoerrorcode() << std::endl;
        std::cout << "protodata: " << person.protodata() << std::endl;


        // 2. 提取外层protodata字段（包含varint长度头 + 内层数据）
        std::string protodata = person.protodata();
        std::vector<uint8_t> inner_data(protodata.begin(), protodata.end());
        tlv::TlvHandler tlvhandler_;
        // 3. 解析varint长度头
        auto result_varint= tlvhandler_.decode_varint(inner_data, 0);
        uint32_t data_len = result_varint.first; 
        uint32_t bytes_read = result_varint.second;
        if (bytes_read == 0 || inner_data.size() < bytes_read + data_len) {
           std::cout << "Invalid inner varint length"<< std::endl;
            return;
        }

        // 4. 提取内层协议数据
        std::vector<uint8_t> inner_payload(
            inner_data.begin() + bytes_read,
            inner_data.begin() + bytes_read + data_len
        );
        if(person.prototype() == 29)
        {
            // 5. 反序列化内层协议
            tutorial::SendMsgInner SendMsgInner;
            SendMsgInner.ParseFromString(std::string(inner_payload.begin(), inner_payload.end()));
            // std::cout << "authState: " << UserAuthRespInner.authstate() << std::endl;
            // std::cout << "ip: " << ServerAddr.ip() << std::endl;
            // std::cout << "authToken: " << UserAuthRespInner.authtoken() << std::endl;
            // Protobuf转JSON
            std::string json;
            JsonPrintOptions options;
            options.add_whitespace = true; // 格式化输出
            MessageToJsonString(person, &json, options);
            std::cout<<"SendMsgInner: " << json << std::endl;            
        }else{
            // 5. 反序列化内层协议
            tutorial::UserAuthRespInner UserAuthRespInner;
            tutorial::ServerAddr ServerAddr;
            UserAuthRespInner.ParseFromString(std::string(inner_payload.begin(), inner_payload.end()));
            // std::cout << "authState: " << UserAuthRespInner.authstate() << std::endl;
            // std::cout << "ip: " << ServerAddr.ip() << std::endl;
            // std::cout << "authToken: " << UserAuthRespInner.authtoken() << std::endl;
            // Protobuf转JSON
            std::string json;
            JsonPrintOptions options;
            options.add_whitespace = true; // 格式化输出
            MessageToJsonString(person, &json, options);
            std::cout << json << std::endl;
        }

        // 这里可以解析Protobuf数据
        // for(int i = 0; i < data.size(); ++i) {
        // std::cout << data[i] <<std::endl;
        // }
    });

    network.setErrorCallback([](int code, const std::string& msg) {
        std::cerr << "[错误] 代码: " << code << " 信息: " << msg << std::endl;
    });

    // 连接服务器
    // if (network.connect("zctxc.crrcgc.cc", 8085)) {
        // if (network.connect("zctpoc.cccar.com.cn", 8085)) {
        tutorial::ConnectOptions options;
        options.host = "zctxc.crrcgc.cc";
        options.port = 8085;
        options.max_retry_count = 5;
        options.retry_interval_sec = 2;
        options.auto_reconnect = true;
        options.heartbeat_interval_sec = 30;
        if (network.connect(options)) {
        // 发送业务数据（示例）
        network.sendMessage(tutorial::ProtocolType::TOKEN_AUTH,MsgLiteInner);
        
        tutorial::SendMsgInner SendMsgInner;
        SendMsgInner.set_msgtype(1);
        SendMsgInner.set_msgid("");
        SendMsgInner.set_msgcontent("hello");
        SendMsgInner.set_msgsender("");
        SendMsgInner.set_msgreceiver("zc68gusp4");
    
        std::string inner_serialized_str_send_msg;
        SendMsgInner.SerializeToString(&inner_serialized_str_send_msg);
        // 创建一个 Person 对象
        tutorial::Person MsgLiteInner11;
        MsgLiteInner11.set_prototype(29);
        MsgLiteInner11.set_protoencrypt(0);
        MsgLiteInner11.set_protodata(inner_serialized_str_send_msg);

        network.sendMessage(tutorial::ProtocolType::IM_DATA,MsgLiteInner11);

        // 保持主线程运行
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return 0;
}
