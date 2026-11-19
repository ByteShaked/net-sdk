# Windows 编译指南

本文档说明如何在 Windows 11 (ARM64/x64) 上编译 IM Network Library。

## 环境要求

- Windows 11 (家庭版/专业版均可，支持 ARM64 和 x64 架构)
- 磁盘空间：至少 15GB 可用空间
- 网络：稳定的互联网连接

---

## 第一步：安装编译工具链

### 1.1 安装 Visual Studio 2022 Build Tools

打开 **PowerShell（管理员模式）**：

```powershell
# 下载 Visual Studio Build Tools 安装器
Invoke-WebRequest -Uri "https://aka.ms/vs/17/release/vs_BuildTools.exe" -OutFile "vs_BuildTools.exe"

# 安装 C++ 编译工具（约需 10-15 分钟）
.\vs_BuildTools.exe --quiet --wait --norestart --nocache `
  --add Microsoft.VisualStudio.Workload.VCTools `
  --add Microsoft.VisualStudio.Component.VC.CMake.Project `
  --includeRecommended
```

> **说明：**
> - `VCTools`：C++ 编译器和标准库
> - `CMake.Project`：CMake 支持
> - 安装完成后可能需要重启计算机

### 1.2 安装 CMake

如果 Build Tools 没有自动安装 CMake，使用 winget 安装：

```powershell
winget install Kitware.CMake
```

验证安装：

```powershell
cmake --version
```

### 1.3 安装 Git

```powershell
winget install Git.Git
```

安装后关闭并重新打开 PowerShell，验证：

```powershell
git --version
```

---

## 第二步：编译 Protobuf 3.19.5

项目依赖 **Protobuf 3.19.5** 版本（必须严格匹配）。

打开 **Developer Command Prompt for VS 2022**（开始菜单搜索）：

```cmd
# 下载 Protobuf 3.19.5 源码
curl -L -o protobuf-3.19.5.tar.gz https://github.com/protocolbuffers/protobuf/releases/download/v3.19.5/protobuf-cpp-3.19.5.tar.gz

# 解压
tar -xzf protobuf-3.19.5.tar.gz
cd protobuf-3.19.5
```

**根据你的架构选择编译参数：**

### ARM64 架构（如 Surface Pro X、联想 ThinkPad X13s）

```cmd
cmake -B build -S cmake -G "Visual Studio 17 2022" -A ARM64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -Dprotobuf_BUILD_TESTS=OFF ^
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF ^
  -DCMAKE_INSTALL_PREFIX=C:\protobuf-3.19.5

cmake --build build --config Release --parallel 4
cmake --install build --config Release
```

### x64 架构（大多数台式机和笔记本）

```cmd
cmake -B build -S cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -Dprotobuf_BUILD_TESTS=OFF ^
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF ^
  -DCMAKE_INSTALL_PREFIX=C:\protobuf-3.19.5

cmake --build build --config Release --parallel 4
cmake --install build --config Release
```

> **编译时间：** 约 5-10 分钟，取决于 CPU 性能

验证安装：

```cmd
C:\protobuf-3.19.5\bin\protoc.exe --version
# 应输出：libprotoc 3.19.5
```

---

## 第三步：安装其他依赖

### 3.1 安装 vcpkg 依赖管理器

```cmd
# 克隆 vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg

# 初始化
.\bootstrap-vcpkg.bat
```

### 3.2 安装 Boost 和 ZLIB

**ARM64 架构：**

```cmd
C:\vcpkg\vcpkg install boost-system:arm64-windows
C:\vcpkg\vcpkg install zlib:arm64-windows
C:\vcpkg\vcpkg integrate install
```

**x64 架构：**

```cmd
C:\vcpkg\vcpkg install boost-system:x64-windows
C:\vcpkg\vcpkg install zlib:x64-windows
C:\vcpkg\vcpkg integrate install
```

> **安装时间：** Boost 约 5-10 分钟，ZLIB 约 1-2 分钟

---

## 第四步：克隆项目并编译

### 4.1 克隆代码

```cmd
# 克隆仓库（或通过虚拟机共享文件夹传输代码）
git clone <你的仓库地址>
cd net-sdk
```

### 4.2 配置 CMake

**ARM64 架构：**

```cmd
cmake -B build-windows -G "Visual Studio 17 2022" -A ARM64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DProtobuf_INCLUDE_DIR=C:\protobuf-3.19.5\include ^
  -DProtobuf_LIBRARIES=C:\protobuf-3.19.5\lib\libprotobuf.lib ^
  -DProtobuf_PROTOC_EXECUTABLE=C:\protobuf-3.19.5\bin\protoc.exe ^
  -DCMAKE_BUILD_TYPE=Release
```

**x64 架构：**

```cmd
cmake -B build-windows -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DProtobuf_INCLUDE_DIR=C:\protobuf-3.19.5\include ^
  -DProtobuf_LIBRARIES=C:\protobuf-3.19.5\lib\libprotobuf.lib ^
  -DProtobuf_PROTOC_EXECUTABLE=C:\protobuf-3.19.5\bin\protoc.exe ^
  -DCMAKE_BUILD_TYPE=Release
```

### 4.3 编译

```cmd
cmake --build build-windows --config Release --parallel 4
```

> **编译时间：** 约 2-5 分钟

### 4.4 检查编译产物

```cmd
dir lib
```

应看到生成的 `network.dll` 和 `network.lib` 文件。

---

## 第五步：运行测试

### 5.1 编译测试程序

如果 `test/` 目录下有测试代码，需要先编译测试程序：

```cmd
cd test
# 根据你的测试程序配置编译
```

### 5.2 运行测试

```cmd
# 假设测试程序为 test_im_signal.exe
.\test_im_signal.exe
```

**验证内容：**
- ✅ 能否连接服务器
- ✅ 认证消息是否正常收发
- ✅ 心跳功能是否正常
- ✅ 协议解析是否正确

---

## 常见问题

### Q1: Protobuf 下载太慢

**方案1：** 在 Mac 上下载好，通过虚拟机共享文件夹传入

**方案2：** 使用国内镜像（如清华、阿里云镜像）

### Q2: vcpkg 安装依赖很慢

**方案：** 配置国内镜像或手动下载预编译包

```cmd
# 设置 vcpkg 二进制缓存
set VCPKG_DEFAULT_BINARY_CACHE=C:\vcpkg_cache
```

### Q3: 编译报错 "Cannot open include file: 'boost/asio.hpp'"

**原因：** vcpkg 依赖未正确安装

**解决：**
1. 检查 vcpkg integrate 是否成功
2. 确认安装的架构匹配（ARM64 vs x64）
3. 重新运行 `vcpkg integrate install`

### Q4: 编译报错 "error C3861: 'mkdir': identifier not found"

**原因：** 代码已修复此问题，确保使用最新代码

**检查：** [NetLog.cpp:6](../src/NetLog.cpp#L6) 应包含：

```cpp
#ifdef _WIN32
#include <direct.h>  // for _mkdir
#endif
```

### Q5: ARM64 和 x64 版本的差异

- **ARM64 版本**：用于 Surface Pro X、联想 ThinkPad X13s 等 ARM 设备
- **x64 版本**：用于大多数台式机和笔记本（Intel/AMD）
- **两者不兼容**：需要根据目标平台选择对应版本

---

## 架构选择指南

### 如何确认当前系统架构？

```powershell
# 查看处理器架构
echo $env:PROCESSOR_ARCHITECTURE

# 输出：
# ARM64 → 编译 ARM64 版本
# AMD64 → 编译 x64 版本
```

### 编译产物命名建议

编译完成后，建议重命名以区分架构：

```cmd
# ARM64 版本
ren lib\network.dll network_arm64.dll

# x64 版本
ren lib\network.dll network_x64.dll
```

---

## 后续步骤

### ARM64 测试通过后

如果在 ARM64 Windows 上测试完全通过：

1. **代码逻辑已验证** ✅
2. **Windows 兼容性 OK** ✅
3. **依赖库配置正确** ✅

**接下来：**
- 使用 GitHub Actions 或云主机编译 x64 版本
- x64 版本有 90%+ 概率直接可用
- 找台 x64 Windows 跑一次冒烟测试即可

### 自动化编译

参考 [GitHub Actions 配置](.github/workflows/build-all-platforms.yml) 实现持续集成。

---

## 参考资料

- [CMakeLists.txt](../CMakeLists.txt) - 项目构建配置
- [Protobuf 3.19.5 Release](https://github.com/protocolbuffers/protobuf/releases/tag/v3.19.5)
- [vcpkg 文档](https://vcpkg.io/)
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/zh-hans/downloads/)
