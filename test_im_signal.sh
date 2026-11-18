#!/bin/bash
# IM Network SDK 信令测试脚本
# 用于测试IM信令的完整编译和运行流程

set -e  # 遇到错误立即退出

echo "============================================"
echo "   IM Network SDK 信令测试脚本"
echo "============================================"
echo ""

# 项目根目录
PROJECT_ROOT="/Users/byte/publicspace/vscode/electron_research/net-sdk"

# 步骤 1: 编译网络库
echo "📦 步骤 1/3: 编译网络库 libnetwork.dylib ..."
echo "--------------------------------------------"
cd "$PROJECT_ROOT"
mkdir -p build
cd build

echo "  ➤ 配置 CMake ..."
cmake .. > /dev/null 2>&1

echo "  ➤ 编译主库 ..."
make -j8
if [ $? -ne 0 ]; then
    echo "❌ 错误: 网络库编译失败"
    exit 1
fi

# 检查库文件是否生成
if [ ! -f "$PROJECT_ROOT/lib/libnetwork.dylib" ]; then
    echo "❌ 错误: libnetwork.dylib 未生成"
    exit 1
fi

echo "✅ 网络库编译成功: $(ls -lh $PROJECT_ROOT/lib/libnetwork.dylib | awk '{print $5}')"
echo ""

# 步骤 2: 编译测试程序
echo "🔧 步骤 2/3: 编译测试程序 minimal_test ..."
echo "--------------------------------------------"
cd "$PROJECT_ROOT/test"
mkdir -p build
cd build

echo "  ➤ 配置测试项目 ..."
cmake .. > /dev/null 2>&1

echo "  ➤ 编译 minimal_test ..."
make -j8
if [ $? -ne 0 ]; then
    echo "❌ 错误: 测试程序编译失败"
    exit 1
fi

# 检查测试程序是否生成
if [ ! -f "./minimal_test" ]; then
    echo "❌ 错误: minimal_test 未生成"
    exit 1
fi

echo "✅ 测试程序编译成功: $(ls -lh ./minimal_test | awk '{print $5}')"
echo ""

# # 步骤 3: 运行测试
# echo "🚀 步骤 3/3: 运行 IM 信令测试 ..."
# echo "--------------------------------------------"

# # 设置动态库搜索路径
# export DYLD_LIBRARY_PATH="$PROJECT_ROOT/lib:$DYLD_LIBRARY_PATH"

# # 显示测试信息
# echo "  ➤ 测试程序: $PROJECT_ROOT/test/build/minimal_test"
# echo "  ➤ 依赖库路径: $PROJECT_ROOT/lib/libnetwork.dylib"
# echo ""
# echo "开始测试..."
# echo "--------------------------------------------"

# # 运行测试
# ./minimal_test

# # 检查返回码
# TEST_RESULT=$?
# echo "--------------------------------------------"
# echo ""

# # 输出测试结果
# if [ $TEST_RESULT -eq 0 ]; then
#     echo "✅ 测试执行完成 (退出码: $TEST_RESULT)"
# else
#     echo "⚠️  测试执行完成，但返回非零退出码: $TEST_RESULT"
# fi

# echo ""
# echo "============================================"
# echo "   测试流程结束"
# echo "============================================"

# exit $TEST_RESULT
