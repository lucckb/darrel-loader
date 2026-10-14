#!/bin/sh
# Usage (inside the bfc1-build container, repo root = darrel-loader):
#   tests/run.sh <u-boot.bin (LZF)> <u-boot-raw.bin>
set -e
cd "$(dirname "$0")/.."
gcc -std=gnu17 -Wall -g -fsanitize=address,undefined -DCONFIG_UBOOT_LZF_COMPRESSED=1 \
    -Itests/shim -Iinclude -Isrc \
    tests/decompress_test.c src/decompress.c src/lzf_d.c -o /tmp/decompress_test
/tmp/decompress_test "$@"
