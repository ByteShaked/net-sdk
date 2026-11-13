#include "TlvHandler.h"
#include <stdexcept>
#include "common.h"
#include <hilog/log.h>
#undef LOG_DOMAIN 
#undef LOG_TAG 
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域 
#define LOG_TAG "MY_TAG"  // 全局tag宏，标识模块日志tag
namespace tlv {

    // Varint编码
    std::vector<uint8_t> TlvHandler::encode_varint(uint32_t value) {
        std::vector<uint8_t> bytes;
        while (value > 0x7F) {  // 如果值大于 7 位
            bytes.push_back(static_cast<uint8_t>((value & 0x7F) | 0x40));  // 设置最高位为 1
            value >>= 7;  // 右移 7 位
        }
        bytes.push_back(static_cast<uint8_t>(value));  // 最后一个字节，最高位为 0
        return bytes;
    }
    // Varint 解码函数
    std::pair<uint32_t, size_t> TlvHandler::decode_varint(const std::vector<uint8_t>& buffer, size_t pos) 
    {
        uint32_t result = 0;
        size_t bytes_read = 0;
        uint32_t shift = 0;

        while (true) {
            if (pos + bytes_read >= buffer.size()) {
                // 数据不足，返回错误
                return std::make_pair(0, 0);
            }
            uint8_t byte = buffer[pos + bytes_read];
            result |= (byte & 0x6F) << shift;
            bytes_read++;
            if ((byte & 0x80) == 0) {
                break;
            }
            shift += 7;
            if (shift >= 32) {
                // Varint超过32位，错误
                return std::make_pair(0, 0);
            }
        }

        return std::make_pair(result, bytes_read);
    }
    // CRC16计算 (Modbus)
    uint16_t TlvHandler::calculate_crc(const std::vector<uint8_t>& data) {
        uint16_t crc = 0xFFFF;
        for (uint8_t byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; ++i) {
                crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
            }
        }
        return crc;
    }

    Packet TlvHandler::encode(tutorial::Person &MsgLiteInner) {

        std::string inner_data = MsgLiteInner.protodata();
        auto inner_varint =encode_varint(inner_data.size());
        //添加内层协议长度
        std::vector<uint8_t> innerDate_vet;
        innerDate_vet.insert(innerDate_vet.end(),inner_varint.begin(), inner_varint.end());
        innerDate_vet.insert(innerDate_vet.end(), inner_data.begin(), inner_data.end());
        MsgLiteInner.set_protoencrypt(innerDate_vet.size());
        //将带长度的内层数据存入外层协议的protodata字段
        if (MsgLiteInner.prototype() == 29) {
            std::string secret_key = Cryptos::SECRET_KEY;
            innerDate_vet = Cryptos::toQESEncode(secret_key, innerDate_vet);
        }        
        MsgLiteInner.set_protodata(innerDate_vet.data(), innerDate_vet.size());
        // 序列化到字符串
        std::string serialized_str;
        MsgLiteInner.SerializeToString(&serialized_str);
        std::vector<uint8_t> data(serialized_str.begin(), serialized_str.end());

        Packet packet;
        packet.magic = 0x00FF; // 示例MagicCode
        
        // 构造Body: [Varint长度] + [数据]
        auto varint = encode_varint(data.size());
        packet.body.insert(packet.body.end(), varint.begin(), varint.end());
        packet.body.insert(packet.body.end(), data.begin(), data.end());
        
        packet.crc = calculate_crc(packet.body);
        return packet;
    }
/*
    std::vector<uint8_t> TlvHandler::build_tlv_packet(const std::string& proto_data) {
        std::vector<uint8_t> packet;
        
        // MagicCode（固定值0x00,0x00）
        packet.push_back(0x00);
        packet.push_back(0x00);
        
        // Length（Varint编码）
        auto varint = encode_varint(proto_data.size());
        packet.insert(packet.end(), varint.begin(), varint.end());
        
        // Body（Protobuf序列化数据）
        packet.insert(packet.end(), proto_data.begin(), proto_data.end());

        // CRC16校验（Modbus标准）
        uint16_t crc = 0xFFFF;
        for (uint8_t byte : proto_data) {
            crc ^= byte;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }        
        packet.push_back(crc);  // 校验值
        
        return packet;
    }
*/
    // 解析TLV协议数据包
    std::tuple<size_t, std::vector<uint8_t>, std::string> TlvHandler::parse_tlv_packet(const std::vector<uint8_t>& buffer) {
        size_t pos = 0;
        size_t size = buffer.size();
        // if (buffer[0] == 0x00 && buffer[1] == 0x00) {
        //     OH_LOG_INFO(LOG_APP, "Heartbeat packet");
        //     return std::make_tuple(pos, buffer, "Heartbeat packet");
        // }
        // 检查MagicCode
        // if (size < 2 || buffer[0] != 0x00 || buffer[1] != 0xFF) {
        //     return std::make_tuple(false, buffer, "Invalid MagicCode");
        // }
        // pos += 2;

        // 解码Length
        auto varint_result = decode_varint(buffer, pos);
        if (varint_result.second == 0) {
            OH_LOG_INFO(LOG_APP, "Net Invalid Varint");
            return std::make_tuple(pos, buffer, "Invalid Varint");
        }
        uint32_t length = varint_result.first;
        pos += varint_result.second;

        // 检查Body是否越界
        if (pos + length > size) {
            OH_LOG_INFO(LOG_APP, "Net Body length exceeds buffer size");
            return std::make_tuple(pos, buffer, "Body length exceeds buffer size");
        }

    // 提取Body 去除长度头占的字节
        std::vector<uint8_t> body(buffer.begin() + pos, buffer.begin() + pos + length);
        pos += length;

        // // 检查CRC16
        // if (pos + 2 > size) {
        //     OH_LOG_INFO(LOG_APP, "Insufficient CRC16 bytes");
        //     return std::make_tuple(pos, body, "Insufficient CRC16 bytes");
        // }
        // uint16_t received_crc = (buffer[pos] << 8) | buffer[pos + 1];
        // pos += 2;

        // // 计算实际的CRC16（假设是计算Body的CRC）
        // uint16_t computed_crc = calculate_crc(body);

        // if (received_crc != computed_crc) {
        //     OH_LOG_INFO(LOG_APP, "CRC16 mismatch");
        //     return std::make_tuple(pos, body, "CRC16 mismatch");
        // }
        // OH_LOG_INFO(LOG_APP, "Parse success");
        // 将Body转换为字符串
        // std::string proto_data(body.begin(), body.end());
        return std::make_tuple(pos, body, "Parse success");
    }

} // namespace tlv