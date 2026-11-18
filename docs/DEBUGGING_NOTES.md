# 调试笔记：修复 IM SDK 连接和认证问题

## 问题背景

在将鸿蒙 IM SDK 迁移到 macOS 进行本地测试时，发现：
1. 连接成功后发送认证消息，服务器无响应（超时）
2. 收到服务器数据后程序陷入死循环，疯狂输出错误日志
3. 心跳功能完全不工作

## 调试过程

### 第一阶段：连接和心跳机制分析

#### 发现 1：心跳线程自动启动
- 代码分析发现 `try_connect()` → `startWorkerThreads()` 会自动启动接收线程和心跳线程
- 不需要手动调用 `network_start_heartbeat()`

#### 发现 2：心跳间隔配置错误
- 测试代码设置 `heartbeat_interval_sec = 30` 秒
- 鸿蒙配置是 `intervalHeartBeat = 5` 秒
- 服务器可能有连接空闲超时机制（约 5-10 秒）
- **修复**：改为 5 秒心跳间隔

#### 发现 3：心跳线程不立即发送
- 心跳线程启动后先 `sleep(heartbeat_interval_sec)`，然后才发送第一个心跳
- 这导致连接后 5 秒内客户端没有发送任何数据
- 服务器可能因此关闭连接

#### 发现 4：服务器主动发送心跳
- 从鸿蒙日志看，服务器会主动向客户端发送心跳包（0x00 0x00）
- 客户端收到后只需 `ignore`，不需要回复
- 服务器心跳间隔也是 5 秒

---

### 第二阶段：认证超时问题

#### 症状
```
send protoclientno : 0
[错误 1009] Timeout for message: 0
认证消息已发送，等待响应...
```

#### 调试步骤

**1. 验证签名计算**
- 用鸿蒙代码验证 MD5 签名计算，结果一致 ✅
- 排除签名问题

**2. 验证服务器响应**
- 用鸿蒙测试 `sig` 故意填错，服务器返回认证失败 ✅
- 说明服务器能正确处理请求并响应
- 但测试代码发送后完全没有响应

**3. 打印编码过程**
添加调试日志：
```cpp
[DEBUG] Inner data size: 174 bytes        // UserAuthInner 序列化
[DEBUG] protoType: 20                     // 消息类型
[DEBUG] Inner data with varint size: 176 bytes  // 加 varint 长度前缀
[DEBUG] Outer serialized size: 186 bytes  // Person 外层协议序列化
[DEBUG] Final packet body size: 188 bytes // 最终包
[DEBUG] Varint for size 186: 7a 01       // ⚠️ 问题！
```

**4. 发现 Varint 编码错误**
- 标准 varint 编码 186 应该是：`ba 01`
- 但实际编码是：`7a 01`
- 差异：`ba` vs `7a` = 0xBA vs 0x7A = 10111010 vs 01111010

**5. 定位代码 Bug**
```cpp
// 错误代码
bytes.push_back(static_cast<uint8_t>((value & 0x7F) | 0x40));  // ❌

// 正确代码
bytes.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));  // ✅
```

**Protobuf Varint 编码规范**：
- 每个字节的低 7 位存储数据
- 最高位（bit 7）是继续位：
  - `1` (0x80) 表示后面还有字节
  - `0` 表示这是最后一个字节
- 代码错误地使用了 `0x40` (bit 6) 作为继续位

**6. 验证修复**
修复后：
```
[DEBUG] Varint for size 186: ba 01 ✅
[DEBUG] Sending 188 bytes, first 32 bytes (hex): ba 01 08 14 12 b0 01 ...
```

---

### 第三阶段：接收死循环问题

#### 症状
修复 varint 后，程序疯狂输出：
```
protoclientno: 0
[INFO]  Net  received_no:  [0][0]
[INFO]  Net  prototype:  [0]
[INFO]  Net Invalid Varint
[INFO]  Net tlv_error_code:  [Invalid Varint]
... (重复数十万次)
```

#### 调试步骤

**1. 分析死循环原因**
- 解析失败返回 `parse_msg_size=0`
- 但缓冲区没有清理
- 下一次循环继续解析同一段错误数据

**2. 发现接收数据插入错误**
```cpp
// 错误代码
buffer.insert(buffer.end(), temp, temp);  // ❌ 什么都没插入

// 正确代码
buffer.insert(buffer.end(), temp, temp + bytes);  // ✅
```

**3. 发现 decode_varint 也有 Bug**
```cpp
// 错误代码
result |= (byte & 0x6F) << shift;  // ❌ 掩码错误

// 正确代码
result |= (byte & 0x7F) << shift;  // ✅
```

继续位检查是对的，但数据位掩码错了。

**4. 完善缓冲区管理**
```cpp
// 解析失败时清空缓冲区
if (parse_msg_size == 0) {
    buffer.clear();
    break;
}

// 解析成功后移除已处理的数据
if (parse_msg_size > 0) {
    buffer.erase(buffer.begin(), buffer.begin() + parse_msg_size);
}

// 缓冲区数据不足，跳出内层循环
if (buffer.empty() || buffer.size() < 2) {
    break;
}
```

**5. 修复心跳包处理**
```cpp
// 错误代码
if (bytes == 2 && temp[0] == 0x00 && temp[1] == 0x00) {
    NET_LOG_INFO("Net Received heartbeat packet, ignore.");
    break;  // ❌ 退出接收循环
}

// 正确代码
if (bytes == 2 && temp[0] == 0x00 && temp[1] == 0x00) {
    NET_LOG_INFO("Net Received heartbeat packet, ignore.");
    continue;  // ✅ 继续接收下一个数据包
}
```

---

### 第四阶段：认证响应解析

#### 症状
```
>>> 认证响应收到！
[警告] 解析认证响应内层数据失败
响应数据长度: 120 字节
```

#### 分析
服务器返回的 `protodata` 格式：`[varint长度] + [UserAuthInner数据]`

因为 `TlvHandler::encode()` 在编码时添加了长度前缀：
```cpp
innerDate_vet.insert(innerDate_vet.end(), inner_varint.begin(), inner_varint.end());
innerDate_vet.insert(innerDate_vet.end(), inner_data.begin(), inner_data.end());
MsgLiteInner.set_protodata(innerDate_vet.data(), innerDate_vet.size());
```

#### 修复
解析时跳过 varint 长度前缀：
```cpp
size_t offset = 0;
uint8_t first_byte = static_cast<uint8_t>(proto_data[0]);
if (first_byte & 0x80) {
    // 多字节 varint
    while (offset < proto_data.size() && (static_cast<uint8_t>(proto_data[offset]) & 0x80)) {
        offset++;
    }
    offset++;
} else {
    // 单字节 varint
    offset = 1;
}
std::string inner_data = proto_data.substr(offset);
authResp.ParseFromString(inner_data);  // ✅ 成功
```

---

## 关键技术要点

### Protobuf Varint 编码

**标准格式**：
- 每个字节的 bit 0-6 存储数据（7 位）
- bit 7 是继续位（MSB - Most Significant Bit）
  - `1` = 后面还有字节
  - `0` = 最后一个字节
- 小端序（低位字节在前）

**编码示例**：
```
值: 186
二进制: 10111010

步骤1: 186 > 0x7F，需要多字节
  第1字节: (186 & 0x7F) | 0x80 = 0x3A | 0x80 = 0xBA
  剩余: 186 >> 7 = 1

步骤2: 1 <= 0x7F，最后一个字节
  第2字节: 1 = 0x01

结果: BA 01
```

**常见错误**：
- ❌ 使用 `0x40` 作为继续位
- ❌ 使用 `0x6F` 作为数据位掩码
- ✅ 正确：`0x80` 继续位，`0x7F` 数据位掩码

### TLV 协议结构

```
发送格式:
[Varint长度] + [外层Protobuf数据]

外层Protobuf (Person):
- protoType: 消息类型 (20=认证, 29=聊天消息)
- protoClientNo: 客户端请求编号
- protoErrorCode: 错误码 (200=成功)
- protoData: 内层数据

内层数据格式:
[Varint长度] + [内层Protobuf数据]
```

### 缓冲区管理模式

```cpp
std::vector<uint8_t> buffer;  // 累积缓冲区

while (true) {
    // 1. 接收数据
    size_t bytes = recv(sockfd, temp, sizeof(temp), 0);
    buffer.insert(buffer.end(), temp, temp + bytes);
    
    // 2. 循环解析多个消息
    while (true) {
        auto result = parse_tlv_packet(buffer);
        size_t parse_msg_size = std::get<0>(result);
        
        if (parse_msg_size == 0) {
            // 解析失败或数据不足
            if (has_error) {
                buffer.clear();  // 清空错误数据
            }
            break;  // 等待更多数据
        }
        
        // 处理消息...
        
        // 3. 移除已处理的数据
        buffer.erase(buffer.begin(), buffer.begin() + parse_msg_size);
        
        if (buffer.size() < 2) {
            break;  // 数据不足，继续接收
        }
    }
}
```

---

## 最终测试结果

```
正在连接 202.103.39.205:8086...
[INFO]  Connected to [202.103.39.205]:[8086]
[INFO]  receiveLoop thread join start !
[INFO]  Heartbeat thread join start !

========== 发送认证消息 ==========
生成认证参数:
  appId: cba0bb291548a8d73fa5502bebe4e64d
  username: cjcjzqwbcs1
  timestamp: 20261118112847
  sig: afff193709e75150b78f48b157269c30

========== 收到服务器响应 ==========
消息类型 (prototype): 20
错误码 (protoerrorcode): 200
>>> 认证响应收到！

认证响应详情:
  authType: 0
  username: 55001_172.19.182.61
  mode: 0

[INFO]  Heartbeat: attempting to send...
[INFO]  Heartbeat sent successfully, 2 bytes
[INFO]  Net Received heartbeat packet, ignore.
[INFO]  Heartbeat: attempting to send...
[INFO]  Heartbeat sent successfully, 2 bytes
```

✅ **所有功能正常工作！**

---

## 经验教训

1. **编码标准很重要**：一个位的错误（0x40 vs 0x80）导致整个协议失效
2. **工具辅助调试**：打印 hex 数据、对比标准格式，快速定位问题
3. **缓冲区管理**：必须正确处理部分数据、解析失败、多消息粘包等情况
4. **跨平台验证**：鸿蒙能工作不代表其他平台也能工作，需要实际测试
5. **日志很关键**：在关键节点添加调试日志，能大大提高调试效率

## 参考资料

- [Protocol Buffers Encoding](https://developers.google.com/protocol-buffers/docs/encoding)
- [Varint 编码详解](https://developers.google.com/protocol-buffers/docs/encoding#varints)
