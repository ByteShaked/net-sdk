#!/bin/bash
# 编译 Protobuf 3.19.5
# 用于解决 net-sdk 项目的 Protobuf 版本不兼容问题
# 前提：protobuf-3.19.5 目录已解压到 net-sdk 同级目录

set -e

echo "============================================"
echo "   编译并安装 Protobuf 3.19.5"
echo "============================================"
echo ""

# 设置路径
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_DIR="$SCRIPT_DIR/../protobuf-3.19.5"
INSTALL_DIR="/opt/protobuf-3.19.5"

# 检查源码目录是否存在
if [ ! -d "$SOURCE_DIR" ]; then
    echo "❌ 错误: 找不到源码目录"
    echo "   期望位置: $SOURCE_DIR"
    echo ""
    echo "请将 protobuf-3.19.5 目录解压到 net-sdk 同级目录"
    exit 1
fi

echo "✅ 找到源码目录: $SOURCE_DIR"
echo ""

echo "🔧 步骤 1/3: 配置编译选项"
echo "--------------------------------------------"
cd "$SOURCE_DIR"

echo "  ➤ 运行 configure..."
./configure --prefix="$INSTALL_DIR" \
    --disable-shared \
    --enable-static \
    CXXFLAGS="-std=c++11 -DNDEBUG -O2"

echo "✅ 配置完成"
echo ""

echo "⚙️  步骤 2/3: 编译 Protobuf (M4 Pro 14核加速)"
echo "--------------------------------------------"
echo "  ➤ 使用 14 核并行编译..."
make -j14
echo "✅ 编译完成"
echo ""

echo "📥 步骤 3/3: 安装到 $INSTALL_DIR"
echo "--------------------------------------------"
echo "  ⚠️  需要 sudo 权限安装到系统目录"
sudo make install
echo "✅ 安装完成"
echo ""

echo "============================================"
echo "   Protobuf 3.19.5 安装成功！"
echo "============================================"
echo ""
echo "安装位置："
echo "  - 头文件: $INSTALL_DIR/include"
echo "  - 库文件: $INSTALL_DIR/lib"
echo "  - protoc: $INSTALL_DIR/bin/protoc"
echo ""
echo "下一步："
echo "  1. 修改 net-sdk/CMakeLists.txt 使用此版本"
echo "  2. 运行测试脚本: ./test_im_signal.sh"
echo ""
