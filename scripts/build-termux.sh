#!/usr/bin/env bash
# scripts/build-termux.sh - Hỗ trợ build repack cho môi trường Termux
set -euo pipefail

PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
CC="${CC:-gcc}"
if command -v clang >/dev/null 2>&1; then
    CC="clang"
fi

echo "==> Đang build wizk-repack cho Termux..."
echo "    Trình biên dịch: ${CC}"
echo "    Prefix:          ${PREFIX}"

make clean
make CC="${CC}" PREFIX="${PREFIX}" CFLAGS="-O2 -Wall -Wextra"

echo "==> Build thành công! Binary tại: ./repack"
echo "    Để cài đặt vào Termux: make install PREFIX=${PREFIX}"
