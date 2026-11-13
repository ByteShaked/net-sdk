# IM Network SDK

基于 TCP + Protobuf 的自研 IM 信令库，用于实现即时通讯的底层网络传输层。

---

## 📋 项目概述

本项目是一个跨平台的 IM 网络信令库，实现了基于 **TCP 长连接 + Protobuf 序列化 + TLV 协议封装**的完整信令系统。

### 核心特性

- ✅ **TCP 长连接管理**：支持连接、断线重连、心跳保活
- ✅ **Protobuf 序列化**：使用 Protocol Buffers v3.19 进行消息序列化
- ✅ **TLV 协议封装**：自定义 Type-Length-Value 协议，包含 CRC16 校验
- ✅ **消息加密**：支持 AES 加密传输（可选）
- ✅ **C 接口导出**：提供纯 C API，方便跨语言调用（已适配鸿蒙 NAPI）
- ✅ **线程安全**：多线程模型（连接线程、接收线程、心跳线程、重连线程）

### 当前平台支持

- ✅ **鸿蒙（HarmonyOS）**：通过 NAPI 桥接，编译为 ARM64 .so 库
- ⚠️ **Linux**：原生支持（当前 CMakeLists.txt 配置）
- ❌ **Windows / macOS / iOS / Android**：需要跨平台改造（见下文）

---

## 🏗️ 项目结构

```
net-sdk/
├── include/                    # 头文件目录
│   ├── NetworkInterface.h      # 网络模块抽象接口（纯虚类）
│   ├── NetworkModule.h         # 核心网络模块（实现类）
│   ├── network_api.h           # C 接口导出（供 NAPI 调用）
│   ├── TlvHandler.h            # TLV 协议编解码器
│   ├── crypto_util.h           # 加密工具（AES）
│   ├── NetLog.h                # 日志模块
│   └── common.h                # 公共常量和错误码
│
├── src/                        # 源文件目录
│   ├── NetworkModule.cpp       # 网络模块核心实现（TCP 连接、收发）
│   ├── network_api.cpp         # C 接口实现（NAPI 桥接层）
│   ├── TlvHandler.cpp          # TLV 协议实现
│   ├── crypto_util.cpp         # 加密实现
│   ├── NetLog.cpp              # 日志实现
│   └── main.cpp                # 测试入口（可选）
│
├── proto_x86_v19/              # Protobuf 协议定义
│   ├── msg2lite.proto          # 消息结构定义
│   └── msg2lite.pb.{h,cc}      # Protobuf 生成代码
│
├── test/                       # 测试代码
│   ├── app.cpp                 # 客户端测试
│   └── server.cpp              # 服务端测试
│
├── CMakeLists.txt              # 构建配置
└── README.md                   # 本文档
```

---

## 🔄 信令流程说明

### 1. 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                 应用层（ArkTS / JS）                      │
│          network_create() / network_connect()           │
└────────────────────┬────────────────────────────────────┘
                     │ C API 调用
┌────────────────────▼────────────────────────────────────┐
│              network_api.cpp（C 接口层）                 │
│     将 C 参数转换为 C++ 对象，调用 NetworkModule         │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│         NetworkModule.cpp（核心网络模块）                │
│  - TCP socket 连接（POSIX API）                         │
│  - 多线程模型（接收线程、心跳线程、重连线程）             │
│  - 通过 TlvHandler 编解码数据包                         │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
┌───────▼─────────┐      ┌────────▼────────┐
│  TlvHandler.cpp │      │ crypto_util.cpp │
│  TLV 协议封装    │      │  AES 加密/解密   │
│  CRC16 校验      │      └─────────────────┘
└───────┬─────────┘
        │
┌───────▼──────────────┐
│ Protobuf 序列化       │
│ msg2lite.pb.cc       │
└──────────────────────┘
        │
        ▼
   TCP Socket 发送/接收
```

### 2. 连接流程

```
用户调用 network_connect()
    ↓
NetworkModule::connect()
    ↓
解析 DNS（Boost.Asio resolver）
    ↓
创建 TCP socket（原生 POSIX API）
    ↓
设置 TCP_NODELAY（禁用 Nagle 算法）
    ↓
connect() 连接服务器
    ↓
启动 3 个工作线程：
    ├─ receiveLoop()      # 接收线程（循环读取 socket）
    ├─ sendHeartbeat()    # 心跳线程（定时发送 PING）
    └─ reconnectManager() # 重连管理线程（失败时触发）
    ↓
连接成功回调 → NetStateCallback(CONNECTED)
```

### 3. 消息发送流程

```
应用层调用 network_send(data, len, msg_type, encrypt)
    ↓
构造 Protobuf 消息（Person 对象）
    outer.set_protodata(data)
    outer.set_prototype(msg_type)
    outer.set_protoencrypt(encrypt)
    ↓
调用 NetworkModule::sendMessage()
    ↓
TlvHandler::encode() 封装 TLV 包：
    ┌──────────────────────────────────┐
    │ Magic (2 bytes) = 0x7E7E         │  # 包头标识
    ├──────────────────────────────────┤
    │ Length (Varint) = body.size()    │  # 变长编码的长度
    ├──────────────────────────────────┤
    │ Body (N bytes)                   │  # Protobuf 序列化后的数据
    │   = outer.SerializeAsString()    │
    ├──────────────────────────────────┤
    │ CRC16 (2 bytes)                  │  # 对 Body 的校验和
    └──────────────────────────────────┘
    ↓
通过 socket send() 发送到服务器
    ↓
如果发送失败 → 触发 ErrorCallback
```

### 4. 消息接收流程

```
receiveLoop() 线程循环执行：
    ↓
1. 读取 Magic（2 字节）
    recv(sockfd_, buffer, 2)
    if (magic != 0x7E7E) → 丢弃
    ↓
2. 读取 Length（Varint 变长编码）
    decode_varint() → 解析出 body_length
    ↓
3. 读取 Body（body_length 字节）
    recv(sockfd_, body_buffer, body_length)
    ↓
4. 读取 CRC16（2 字节）
    recv(sockfd_, crc_buffer, 2)
    ↓
5. 校验 CRC
    calculate_crc(body) == received_crc
    if 不匹配 → 丢弃包，报错
    ↓
6. 反序列化 Protobuf
    Person outer;
    outer.ParseFromArray(body)
    ↓
7. 回调给应用层
    DataCallback(outer.protodata())
```

### 5. 心跳机制

```
startHeartbeat() 启动心跳线程
    ↓
每隔 heartbeat_interval_sec_ 秒：
    ↓
构造心跳包（空消息，msg_type = 0xFF）
    ↓
调用 sendMessage() 发送
    ↓
服务端应返回心跳响应（可选）
    ↓
如果长时间无响应 → 触发重连
```

### 6. 断线重连流程

```
接收线程检测到 socket 断开（recv() 返回 0）
    ↓
设置状态 → ConnectionState::RECONNECTING
    ↓
触发 reconnect_cv_.notify_all()
    ↓
reconnectManager() 线程被唤醒
    ↓
循环重连（最多 max_retry_count_ 次）：
    ├─ 等待 retry_interval_sec_ 秒
    ├─ 调用 try_connect()
    └─ 如果成功 → 恢复工作线程
    ↓
如果全部失败 → ErrorCallback(NETWORK_ERR_RECONNECT_FAILED)
```

---

## 📂 核心文件说明

### 1. [NetworkInterface.h](include/NetworkInterface.h)
**作用**：网络模块的抽象接口（纯虚类）

- 定义了 IM 网络层的标准接口：`connect()`, `sendMessage()`, `disconnect()`
- 定义回调类型：`DataCallback`（接收消息）、`ErrorCallback`（错误通知）、`NetStateCallback`（连接状态变化）
- 定义协议类型枚举：`ProtocolType`（FULL_AUTH、TOKEN_AUTH、IM_DATA）
- 定义连接状态枚举：`ConnectionState`（DISCONNECTED、CONNECTING、CONNECTED、RECONNECTING）

### 2. [NetworkModule.h](include/NetworkModule.h) / [NetworkModule.cpp](src/NetworkModule.cpp)
**作用**：核心网络模块实现

**关键成员变量：**
- `sockfd_`：TCP socket 文件描述符
- `is_connected_`：连接状态原子标志
- `receive_thread_`：接收线程
- `heartbeat_thread_`：心跳线程
- `reconnect_manager_thread_`：重连管理线程
- `data_callback_`：消息接收回调
- `error_callback_`：错误回调
- `net_state_callback_`：连接状态回调

**关键方法：**
- `connect()`：建立 TCP 连接，启动工作线程
- `try_connect()`：底层连接逻辑（DNS 解析、socket 创建、connect）
- `sendMessage()`：发送消息（TLV 封装 + socket send）
- `receiveLoop()`：接收线程主循环（阻塞读取 + 解析 TLV）
- `sendHeartbeat()`：心跳线程主循环
- `reconnect()`：重连逻辑
- `disconnect()`：断开连接，停止所有线程

**平台依赖点（需要改造）：**
- [NetworkModule.cpp:2](src/NetworkModule.cpp#L2)：`#include <arpa/inet.h>`（POSIX 特定）
- [NetworkModule.cpp:3](src/NetworkModule.cpp#L3)：`#include <unistd.h>`（POSIX 特定）
- [NetworkModule.cpp:9](src/NetworkModule.cpp#L9)：`#include <hilog/log.h>`（鸿蒙特定）
- [NetworkModule.cpp:85](src/NetworkModule.cpp#L85)：原生 socket API（`socket()`, `setsockopt()`, `connect()`）
- [NetworkModule.cpp:94](src/NetworkModule.cpp#L94)：`close(sock)`（Windows 是 `closesocket()`）

### 3. [network_api.h](include/network_api.h) / [network_api.cpp](src/network_api.cpp)
**作用**：C 接口导出层（供 NAPI 调用）

将 C++ 的 `NetworkModule` 类封装成纯 C API：

```c
NetworkHandle network_create();                  // 创建实例
int network_connect(NetworkHandle, options);     // 连接服务器
int network_send(NetworkHandle, data, len);      // 发送消息
void network_set_callbacks(handle, cb, userdata);// 设置回调
void network_disconnect(NetworkHandle);          // 断开连接
void network_destroy(NetworkHandle);             // 销毁实例
```

**为什么需要这一层？**
- NAPI（Node-API）只能调用 C 接口，不能直接调用 C++ 类
- 这一层负责 C ↔ C++ 对象的转换（Handle ↔ NetworkModule*）
- 跨平台性：同样的 C API 可以适配到 Electron N-API、Android JNI、iOS Objective-C++

### 4. [TlvHandler.h](include/TlvHandler.h) / [TlvHandler.cpp](src/TlvHandler.cpp)
**作用**：TLV 协议编解码器

**TLV 包格式：**
```
┌─────────────┬─────────────┬─────────────┬─────────────┐
│ Magic       │ Length      │ Body        │ CRC16       │
│ (2 bytes)   │ (Varint)    │ (N bytes)   │ (2 bytes)   │
│ 0x7E7E      │ 变长编码     │ Protobuf    │ 校验和       │
└─────────────┴─────────────┴─────────────┴─────────────┘
```

**关键方法：**
- `encode(Person&)`：将 Protobuf 消息编码成 TLV 包
- `parse_tlv_packet(buffer)`：从字节流解析 TLV 包
- `encode_varint(uint32_t)`：变长整数编码（节省空间）
- `decode_varint(buffer, pos)`：变长整数解码
- `calculate_crc(data)`：CRC16 校验和计算（Modbus 标准）

### 5. [crypto_util.h](include/crypto_util.h) / [crypto_util.cpp](src/crypto_util.cpp)
**作用**：消息加密/解密工具

- 实现 AES 加密/解密（CBC 模式）
- 当 `Person.protoencrypt() == 1` 时，对 `protodata` 进行加密

### 6. [common.h](include/common.h)
**作用**：公共常量和错误码定义

**错误码：**
- `1001-1017`：网络模块错误（连接失败、超时、DNS 失败等）
- `2001-2005`：TLV 协议错误（Magic 错误、CRC 校验失败等）

### 7. [proto_x86_v19/msg2lite.proto](proto_x86_v19/msg2lite.proto)
**作用**：Protobuf 消息定义

定义了 `Person` 消息结构（外层消息包装器）：
```protobuf
message Person {
    int32 prototype = 1;       // 消息类型（业务协议号）
    bytes protodata = 2;       // 消息体（业务 Protobuf 序列化后的数据）
    int32 protoencrypt = 3;    // 是否加密（0=明文，1=AES）
}
```

**编译生成：**
```bash
protoc --cpp_out=. msg2lite.proto
# 生成 msg2lite.pb.h 和 msg2lite.pb.cc
```

---

## 🛠️ 环境依赖（当前 Linux/鸿蒙版本）

### 必需依赖

| 依赖项 | 版本要求 | 用途 | 安装方式 |
|---|---|---|---|
| **CMake** | >= 2.6 | 构建工具 | `brew install cmake` (macOS)<br>`apt install cmake` (Linux) |
| **GCC/Clang** | C++11 支持 | C++ 编译器 | 系统自带 |
| **Protobuf** | 3.19.x | 消息序列化 | `brew install protobuf@3.19` (macOS)<br>或手动编译安装到 `/usr/local/protobuf_v19/` |
| **Boost** | >= 1.58 | Asio 网络库（DNS 解析） | `brew install boost` (macOS)<br>`apt install libboost-all-dev` (Linux) |
| **pthread** | POSIX 标准 | 多线程 | Linux/macOS 自带 |
| **HiLog** | 鸿蒙 SDK | 日志库（仅鸿蒙） | 鸿蒙 DevEco Studio 自带 |

### macOS 环境安装

```bash
# 1. 安装 Xcode Command Line Tools
xcode-select --install

# 2. 安装 Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 3. 安装依赖
brew install cmake
brew install boost
brew install protobuf@3.19  # 或直接 brew install protobuf（最新版）
brew install node           # 如果需要编译 Electron N-API addon

# 4. 验证安装
cmake --version
c++ --version
protoc --version
ls /opt/homebrew/include/boost  # 验证 Boost 头文件
```

### Linux 环境安装

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake libboost-all-dev libprotobuf-dev protobuf-compiler

# CentOS/RHEL
sudo yum install gcc-c++ cmake boost-devel protobuf-devel
```

---

## 🔨 编译步骤

### 当前版本（Linux/鸿蒙）

```bash
cd net-sdk
mkdir build && cd build

# 配置（需要手动指定 Protobuf 和 Boost 路径）
cmake .. \
  -DPROTO=/usr/local/protobuf_v19 \
  -DBOOST=/usr/local/boost

# 编译
make

# 产物
ls lib/libnetwork.so    # 动态库（供 NAPI 调用）
ls bin/im_network       # 可执行文件（测试用）
```

### 已知问题

1. **硬编码路径**：[CMakeLists.txt:49-50](CMakeLists.txt#L49-L50) 写死了 `/usr/local/protobuf_v19/` 和 `/usr/local/boost/`
2. **只支持 Linux**：[CMakeLists.txt:47](CMakeLists.txt#L47) 定义了 `-D LINUX` 宏
3. **GCC 版本限制**：[CMakeLists.txt:7-9](CMakeLists.txt#L7-L9) 要求 GCC < 5.0（过时的限制）

---

## ⚠️ 跨平台改造需求

### 当前平台依赖问题

| 问题 | 影响平台 | 改造方案 |
|---|---|---|
| **原生 socket API** | Windows 无法编译 | 替换成 Boost.Asio |
| **`<arpa/inet.h>`** | Windows 无法编译 | 条件编译或用 Boost |
| **`<hilog/log.h>`** | 仅鸿蒙有 | 条件编译宏 |
| **硬编码路径** | macOS/Windows 路径不同 | 改用 `find_package()` |

### 改造工作量评估

| 工作项 | 预计耗时 |
|---|---|
| 替换 socket API → Boost.Asio | 1-2 天 |
| 日志系统跨平台（条件编译） | 0.5 天 |
| CMakeLists.txt 改造 | 1 天 |
| macOS 本地验证 | 0.5 天 |
| **总计** | **3-4 天** |

---

## 📝 使用示例

### C API 调用示例（供 NAPI 参考）

```c
#include "network_api.h"

// 1. 创建实例
NetworkHandle handle = network_create();

// 2. 设置回调
void on_data(const char* data, int len, void* userdata) {
    printf("收到消息: %.*s\n", len, data);
}

void on_error(int code, const char* msg, void* userdata) {
    printf("错误: [%d] %s\n", code, msg);
}

network_set_callbacks(handle, on_data, on_error, NULL);

// 3. 连接服务器
ConnectOptionsC options = {
    .host = "im.example.com",
    .port = 8888,
    .max_retry_count = 3,
    .retry_interval_sec = 2,
    .auto_reconnect = true,
    .heartbeat_interval_sec = 20
};
int ret = network_connect(handle, &options);

// 4. 发送消息
const char* msg = "Hello, IM!";
network_send(handle, msg, strlen(msg), 0x01, 0);  // msg_type=1, 不加密

// 5. 断开连接
network_disconnect(handle);
network_destroy(handle);
```

---

## 🔜 后续计划

1. **跨平台改造**：替换 socket API 为 Boost.Asio，支持 Windows/macOS/Linux
2. **移动端适配**：编译 Android .so / iOS .a，桥接到 JNI / Objective-C++
3. **Electron 集成**：编译 N-API addon，供 Electron 主进程调用
4. **CI/CD**：GitHub Actions 自动化跨平台编译

---

## 📧 联系方式

如有问题，请联系项目维护者。
