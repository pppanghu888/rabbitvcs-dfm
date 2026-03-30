# SVN checkout vs export 详解

## 问题现象
使用 RabbitVCS 导入 `svn://localhost/svn-test` 时，发现下载了大量文件，但实际仓库中只有几个文件。

## 根本原因

### 1. SVN 工作副本的结构

SVN checkout 后的目录结构：
```
svn-test/
├── .svn/                           # ← 隐藏的元数据目录
│   ├── pristine/                   # 所有文件的原始副本
│   │   ├── 12/34abcd...            # file1.py 的原始版本
│   │   ├── 56/78efgh...            # file2.py 的原始版本
│   │   └── ...                     # 更多文件
│   ├── wc.db                       # SQLite 数据库
│   ├── tmp/                        # 临时文件
│   └── prop-base/                  # 属性文件
├── actual-file1.py                 # 实际项目文件
├── actual-file2.py                 # 实际项目文件
└── README.md                       # 实际项目文件
```

### 2. 文件数量对比

```bash
# 查看工作副本总文件数（包含 .svn）
find . -type f | wc -l
# 输出：可能是 100+ 个文件

# 查看实际项目文件数（排除 .svn）
find . -type f -not -path "*/.svn/*" | wc -l
# 输出：只有 3 个文件

# 查看 .svn 目录中的文件数
find . -type f -path "*/.svn/*" | wc -l
# 输出：97 个文件
```

## Checkout vs Export 对比

### svn checkout
```bash
svn checkout svn://localhost/svn-test
```

**特点：**
- ✅ 创建工作副本（可以编辑、提交）
- ✅ 包含完整的 `.svn` 元数据目录
- ✅ 支持 `svn commit`、`svn update` 等操作
- ❌ 磁盘占用大（每个文件有 2 份副本）
- ❌ 下载时间长

**适用场景：**
- 需要修改代码并提交
- 需要查看历史记录
- 需要更新到最新版本

### svn export
```bash
svn export svn://localhost/svn-test
```

**特点：**
- ✅ 只导出纯净的项目文件
- ✅ 不包含 `.svn` 目录
- ✅ 磁盘占用小
- ✅ 下载速度快
- ❌ 无法提交更新
- ❌ 不包含版本控制信息

**适用场景：**
- 发布生产代码
- 创建干净的备份
- 分享源代码压缩包
- 只需要查看代码

## 与 Git 的对比

### Git 的工作目录
```bash
git clone https://github.com/user/repo.git
```

**目录结构：**
```
repo/
├── .git/                          # 单个目录存储所有元数据
│   ├── objects/                   # 所有历史版本的对象
│   ├── refs/                      # 分支和标签引用
│   └── ...                        # 其他元数据
├── file1.py                       # 实际文件（只有一份）
├── file2.py
└── README.md
```

**关键差异：**
- Git 只在 `.git` 目录存储元数据
- 工作目录中的文件是实际的文件，不是副本
- 可以轻松创建多个工作目录

### Git 的"导出"等效操作
```bash
# 方法 1：git archive（推荐）
git archive --format=tar --output=repo.tar HEAD
git archive --format=zip --output=repo.zip HEAD

# 方法 2：克隆后删除 .git
git clone https://github.com/user/repo.git
rm -rf repo/.git

# 方法 3：稀疏检出
git clone --depth 1 --no-checkout https://github.com/user/repo.git
cd repo
git checkout HEAD -- file1.py file2.py
```

## 实用命令

### SVN 命令

```bash
# 查看远程仓库的文件列表（不下载）
svn list svn://localhost/svn-test
svn list -R svn://localhost/svn-test  # 递归列出

# 导出干净的代码
svn export svn://localhost/svn-test /path/to/export

# 只检出特定目录
svn checkout svn://localhost/svn-test/path/to/dir

# 查看工作副本大小
du -sh ./
du -sh ./.svn

# 统计文件数量
find . -type f -not -path "*/.svn/*" | wc -l
```

### Git 命令

```bash
# 查看远程文件（不克隆）
git ls-remote --heads https://github.com/user/repo.git

# 获取文件内容（不克隆）
git archive --remote=https://github.com/user/repo.git HEAD path/to/file | tar -x

# 浅克隆（只获取最新版本）
git clone --depth 1 https://github.com/user/repo.git

# 稀疏检出（只检出特定目录）
git clone --no-checkout https://github.com/user/repo.git
cd repo
git sparse-checkout init
git sparse-checkout set src/lib
git checkout

# 导出干净代码
git archive --format=zip --output=repo.zip HEAD
```

## 性能对比示例

### SVN
```bash
# 仓库信息：100 个文件，每个文件平均 10KB

# Checkout（包含 .svn）
svn checkout svn://localhost/svn-test
# 下载大小：约 20MB（每个文件 2 份）
# 文件数量：约 300 个（100 个项目文件 + 200 个元数据文件）
# 时间：约 10 秒

# Export（纯净）
svn export svn://localhost/svn-test
# 下载大小：约 1MB（只有项目文件）
# 文件数量：100 个
# 时间：约 2 秒
```

### Git
```bash
# 仓库信息：100 个文件，每个文件平均 10KB，100 次提交

# Clone（完整历史）
git clone https://github.com/user/repo.git
# 下载大小：约 5MB（压缩后的历史）
# 文件数量：100 个（工作目录）+ N 个（.git 对象）
# 时间：约 5 秒

# Shallow Clone（只有最新版本）
git clone --depth 1 https://github.com/user/repo.git
# 下载大小：约 1MB
# 文件数量：100 个
# 时间：约 1 秒
```

## RabbitVCS 使用建议

### 如果只是查看代码
1. 右键 → SVN → Export
2. 选择导出路径
3. 得到干净的代码副本

### 如果需要编辑和提交
1. 右键 → SVN → Checkout
2. 接受会下载 `.svn` 目录
3. 可以正常使用版本控制功能

### 清理工作副本
```bash
# 删除 .svn 目录（小心操作）
find /path/to/working_copy -type d -name ".svn" -exec rm -rf {} +

# 或使用 SVN 清理命令
svn cleanup /path/to/working_copy
```

## 总结

| 特性 | SVN Checkout | SVN Export | Git Clone |
|------|-------------|------------|-----------|
| **元数据** | 包含 .svn 目录 | 无元数据 | 包含 .git 目录 |
| **文件数量** | 实际文件 × 2 | 只有实际文件 | 只有实际文件 |
| **可提交** | ✅ 是 | ❌ 否 | ✅ 是 |
| **磁盘占用** | 大 | 小 | 中 |
| **下载速度** | 慢 | 快 | 中 |
| **适用场景** | 开发环境 | 发布/查看 | 开发环境 |

---

**建议：**
- 只需查看代码 → 使用 `svn export`
- 需要开发修改 → 使用 `svn checkout`，接受 `.svn` 目录
- 发布生产代码 → 使用 `svn export` 或 Git 的 `git archive`
