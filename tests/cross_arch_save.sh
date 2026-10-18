#!/bin/bash
set -ex

ARCH=$1
export DEBIAN_FRONTEND=noninteractive

# 1. Install dependencies for minimal build
apt-get update
apt-get install -y build-essential meson ninja-build gettext pkg-config e2fslibs-dev uuid-dev libblkid-dev libxxhash-dev libssl-dev zlib1g-dev libzstd-dev e2fsprogs scdoc

# 2. Compile partclone (minimal ext4 build to save CI time on emulated architectures)
cd /workspace
# 確保上一次編譯架構遺留下來的 build 目錄被徹底清除乾淨
rm -rf build

meson setup build -Dextfs=enabled -Dxxhash=true --prefix=/usr
meson compile -C build

# 3. Create dummy ext4 filesystem
cd /workspace
truncate -s 50M base_${ARCH}.img
mkfs.ext4 -F base_${ARCH}.img

# 4. Mount and populate with random data (to ensure xxhash128bit is fully tested on payload data)
mkdir -p /mnt/base
mount -o loop base_${ARCH}.img /mnt/base
# Generate random data and write it to the image
dd if=/dev/urandom of=/mnt/base/random_data.bin bs=1M count=10

# Record the MD5 of the random data as the gold standard for restoration
md5sum /mnt/base/random_data.bin | awk '{print $1}' > /workspace/payload_md5_${ARCH}.txt

umount /mnt/base

# 5. Save image using partclone
build/src/partclone.extfs -c -s base_${ARCH}.img -O test_img_${ARCH}.img -F -q -a3

# Fix permissions to allow non-root users (like CI runners) to read the artifact
chmod a+r test_img_${ARCH}.img /workspace/payload_md5_${ARCH}.txt

# Cleanup dummy source to save space
rm base_${ARCH}.img
