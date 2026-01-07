#!/bin/bash
# Client端快速构建脚本

set -e

echo "========================================="
echo "  论文审稿系统 Client 构建脚本"
echo "========================================="

# 创建构建目录
if [ ! -d "build" ]; then
    echo "创建构建目录..."
    mkdir build
fi

cd build

# 配置CMake
echo "配置CMake..."
cmake ..

# 编译
echo "编译中..."
make

echo ""
echo "========================================="
echo "  构建成功!"
echo "========================================="
echo "可执行文件: ./build/client"
echo ""
echo "运行方式:"
echo "  ./build/client                    # 连接到 localhost:8080"
echo "  ./build/client -h HOST -p PORT    # 指定服务器地址"
echo "  ./build/client --help             # 查看帮助"
echo ""

