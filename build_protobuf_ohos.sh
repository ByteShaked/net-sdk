#!/bin/bash
# 为鸿蒙交叉编译 Protobuf 3.19.5

set -e

echo "========================================="
echo "为鸿蒙交叉编译 Protobuf 3.19.5"
echo "========================================="

# 鸿蒙 SDK 路径
OHOS_SDK_NATIVE="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native"
OHOS_SYSROOT="${OHOS_SDK_NATIVE}/sysroot"
OHOS_TOOLCHAIN="${OHOS_SDK_NATIVE}/llvm"

# Protobuf 源码目录
PROTOBUF_SRC="/Users/byte/publicspace/vscode/electron_research/protobuf-3.19.5"
if [ ! -d "$PROTOBUF_SRC" ]; then
    echo "错误：找不到 Protobuf 源码目录: $PROTOBUF_SRC"
    exit 1
fi

# 安装目录
INSTALL_PREFIX="/opt/protobuf-3.19.5-ohos"

# 清理旧的构建
cd "$PROTOBUF_SRC"
rm -rf build_ohos
mkdir -p build_ohos
cd build_ohos

echo "配置 Protobuf（鸿蒙交叉编译）..."

# 设置交叉编译环境变量
export CC="${OHOS_TOOLCHAIN}/bin/clang"
export CXX="${OHOS_TOOLCHAIN}/bin/clang++"
export AR="${OHOS_TOOLCHAIN}/bin/llvm-ar"
export RANLIB="${OHOS_TOOLCHAIN}/bin/llvm-ranlib"
export CFLAGS="-target aarch64-unknown-linux-ohos --sysroot=${OHOS_SYSROOT} -fPIC"
export CXXFLAGS="-target aarch64-unknown-linux-ohos --sysroot=${OHOS_SYSROOT} -fPIC -std=c++11"
export LDFLAGS="-target aarch64-unknown-linux-ohos --sysroot=${OHOS_SYSROOT}"

# 配置 Protobuf
../configure \
    --prefix="$INSTALL_PREFIX" \
    --host=aarch64-unknown-linux-ohos \
    --with-sysroot="$OHOS_SYSROOT" \
    --disable-shared \
    --enable-static \
    --with-protoc=/opt/protobuf-3.19.5/bin/protoc

echo "编译 Protobuf..."
make -j14

echo "安装 Protobuf..."
sudo make install

echo ""
echo "========================================="
echo "Protobuf 3.19.5 鸿蒙版本编译完成！"
echo "========================================="
echo "安装路径: $INSTALL_PREFIX"
echo ""
ls -lh "$INSTALL_PREFIX/lib/libprotobuf.a"
