#pragma once
#include <vector>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <string>
#include <algorithm>
#include "msg/MsgLite.pb.h"
#include "crypto_util.h"

// 兼容旧代码：旧版 proto 里消息叫 Person，新版叫 MsgLiteInner
namespace tutorial {
    using Person = MsgLiteInner;
}
namespace tlv {

    struct Packet {
        uint16_t magic;
        std::vector<uint8_t> body;
        uint16_t crc;
    };

    class TlvHandler {
    public:
        static Packet encode(tutorial::Person &MsgLiteInner);
        // std::vector<uint8_t> build_tlv_packet(const std::string& proto_data);
        std::tuple<size_t, std::vector<uint8_t>, std::string> parse_tlv_packet(const std::vector<uint8_t>& buffer);
        std::pair<uint32_t, size_t> decode_varint(const std::vector<uint8_t>& buffer, size_t pos);

    private:
        static std::vector<uint8_t> encode_varint(uint32_t value);
        
        static uint16_t calculate_crc(const std::vector<uint8_t>& data);
    };

} // namespace tlv
