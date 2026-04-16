#!/bin/bash
set -e

cd "$(dirname "$0")"   # 进入脚本所在目录（项目根目录）

rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)

echo "Build successful. Executable: ./build/my_http"