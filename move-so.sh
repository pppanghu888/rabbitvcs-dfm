#!/bin/bash

# 移动 build 目录中的 so 文件到目标目录
# 目标目录: /home/ljc/project/dde-file-manager/gerrit/dde-file-manager/build/src/plugins/extensions

SOURCE_SO="./build/librabbitvcs-dfm.so"
TARGET_DIR="~/project/dde-file-manager/gerrit/dde-file-manager/build/src/plugins/extensions"

# 检查源文件是否存在
if [ ! -f "$SOURCE_SO" ]; then
    echo "错误: 源文件 $SOURCE_SO 不存在"
    exit 1
fi

# 创建目标目录（如果不存在）
mkdir -p "$TARGET_DIR"

# 移动 so 文件
echo "正在移动 $SOURCE_SO 到 $TARGET_DIR ..."
cp "$SOURCE_SO" "$TARGET_DIR/"

if [ $? -eq 0 ]; then
    echo "成功: 文件已移动到 $TARGET_DIR/librabbitvcs-dfm.so"
    ls -lh "$TARGET_DIR/librabbitvcs-dfm.so"
else
    echo "错误: 文件移动失败"
    exit 1
fi
