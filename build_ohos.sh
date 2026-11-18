#!/bin/bash
# 鸿蒙交叉编译脚本 - 编译 ARM64 .so 库

set -e  # 遇到错误立即退出

echo "========================================="
echo "开始编译鸿蒙 ARM64 .so 库"
echo "========================================="

# 项目根目录
PROJECT_ROOT=$(cd "$(dirname "$0")" && pwd)
cd "$PROJECT_ROOT"

# 清理旧的构建目录
echo "清理旧的构建目录..."
rm -rf build_ohos
mkdir -p build_ohos
cd build_ohos

# 配置 CMake（使用鸿蒙工具链）
echo "配置 CMake（鸿蒙工具链）..."
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../ohos-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DPROTO=/opt/protobuf-3.19.5 \
  -DBOOST=/opt/homebrew

# 编译（使用 14 核心）
echo "开始编译..."
make -j14

echo ""
echo "========================================="
echo "编译完成！"
echo "========================================="
echo ""
echo "输出文件："
echo "  动态库: $(pwd)/lib/libnetwork.so"
echo ""

# 检查输出文件
if [ -f "lib/libnetwork.so" ]; then
    echo "✅ libnetwork.so 编译成功"
    echo "文件大小: $(du -h lib/libnetwork.so | cut -f1)"
    echo "文件信息:"
    file lib/libnetwork.so
else
    echo "❌ libnetwork.so 编译失败"
    exit 1
fi

echo ""
echo "部署到鸿蒙项目："
echo "  1. 将 lib/libnetwork.so 复制到鸿蒙项目的 entry/libs/arm64-v8a/ 目录"
echo "  2. 确保 CMakeLists.txt 中有："
echo "     target_link_libraries(entry PUBLIC libnetwork.so)"
