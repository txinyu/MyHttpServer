#!/bin/bash
set -e

cd "$(dirname "$0")"   # 进入脚本所在目录，也就是项目根目录

rm -rf build
mkdir build
cd build

cmake ..
make -j$(nproc)

echo ""
echo "Build successful."
echo "Server executable: ./build/bin/my_http"
echo "Bench executables:"
echo "  ./build/bin/bench"
echo "  ./build/bin/bench_keepalive"
echo "  ./build/bin/latency_bench"
echo ""
echo "Run server:"
echo "  ./build/bin/my_http"
echo ""
echo "Run latency benchmark:"
echo "  ./build/bin/latency_bench"