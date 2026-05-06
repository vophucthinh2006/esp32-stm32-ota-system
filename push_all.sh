#!/bin/bash

SOURCE_STM_BIN="/d/ML&IoT Lab/Embedded C programming - MLIoT Lab/BOOTLOADER/BOOTLOADER_TESTING/Debug/BOOTLOADER_TESTING.bin"

echo "Copying newest file .bin"
# Copy file bin cua STM vao thu muc Git
cp "$SOURCE_STM_BIN" "."

echo "Pushing to GitHub"
git add .

msg="Update firmware $(date +'%Y-%m-%d %H:%M:%S')"
git commit -m "$msg"

git push origin main

echo "Update is successful!"