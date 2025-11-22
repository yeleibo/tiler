# 任务恢复功能说明

## 功能概述

程序现在支持**任务恢复**功能，可以在关闭程序后重新启动时从上次中断的地方继续下载，避免重复下载已完成的瓦片。

## 配置选项

在 `conf.toml` 中添加了新的配置项：

```toml
[task]
    # 是否启用任务恢复（true=启用, false=禁用）
    resume = true
```

### 选项说明：

- **`resume = true`**: 启用任务恢复
  - 程序会创建一个进度数据库 (`*.progress.db`)
  - 每下载完一个瓦片就记录到数据库
  - 重启后会跳过已记录的瓦片

- **`resume = false`**: 禁用任务恢复
  - 不创建进度数据库
  - 每次启动都从头开始

## 工作原理

### 1. 进度数据库

程序会在输出目录创建进度数据库文件：
```
output/
  ├── wuhan-google-satelite-z0-20.progress.db  # 进度数据库
  ├── wuhan-google-satelite-z0-20/             # 瓦片输出目录
  └── ...
```

### 2. 下载流程

```
开始下载
  ↓
检查进度数据库
  ↓
瓦片已在进度DB? → 是 → 跳过
  ↓ 否
瓦片已存在文件? → 是 → 跳过并记录到进度DB
  ↓ 否
下载瓦片
  ↓
保存到文件/数据库
  ↓
记录到进度数据库 ✓
```

### 3. 恢复逻辑

当重新启动程序时：
1. 读取进度数据库
2. 对每个待下载瓦片检查：
   - 是否在进度数据库中 → 跳过
   - 是否存在于输出文件 → 跳过（并标记到进度DB）
   - 都不存在 → 下载

## 使用示例

### 场景 1: 首次下载

```toml
[task]
    resume = true
```

**行为**:
- 创建新的进度数据库
- 下载所有瓦片
- 每个瓦片下载后立即记录

### 场景 2: 中断后恢复

```bash
# 第一次运行
./tiler.exe -c conf.toml
# ... 下载到一半，按 Ctrl+C 中断

# 第二次运行
./tiler.exe -c conf.toml
# 自动跳过已下载的瓦片，继续下载剩余部分
```

**输出日志**:
```
[INFO] Progress tracking enabled, task can be resumed after interruption
[DEBUG] tile(z:10, x:123, y:456) already downloaded (from progress), skipping...
[INFO] tile(z:10, x:124, y:456), 150ms, 45.2 kb...
```

### 场景 3: 重新开始全新下载

```toml
[task]
    resume = false  # 禁用恢复
```

**行为**:
- 删除旧的进度数据库
- 从头开始下载所有瓦片

## 与 skipexisting 的区别

| 功能 | resume | skipexisting |
|------|--------|--------------|
| **目的** | 记录下载进度，支持中断后恢复 | 跳过已存在的瓦片文件 |
| **检查方式** | 查询进度数据库 | 检查文件系统/MBTiles |
| **使用场景** | 长时间下载任务，可能中断 | 避免重复下载已存在的瓦片 |
| **性能** | 数据库查询（快） | 文件系统/数据库查询 |
| **推荐配置** | `resume = true` | `skipexisting = true` |

### 推荐配置组合

```toml
[task]
    # 同时启用两个功能，获得最佳体验
    skipexisting = true   # 跳过已存在的文件
    resume = true         # 记录下载进度
```

**效果**:
- 重启后从进度数据库快速跳过已下载瓦片
- 对于已存在但未记录的瓦片，检查文件并补充记录
- 双重保护，确保不重复下载

## 进度数据库结构

```sql
CREATE TABLE downloaded_tiles (
    z INTEGER NOT NULL,          -- 缩放级别
    x INTEGER NOT NULL,          -- X坐标
    y INTEGER NOT NULL,          -- Y坐标
    downloaded_at DATETIME,      -- 下载时间
    PRIMARY KEY (z, x, y)
);

CREATE INDEX idx_tile ON downloaded_tiles(z, x, y);
```

## 数据库文件管理

### 自动管理
- **启用 `resume = true`**: 保留进度数据库，支持恢复
- **禁用 `resume = false`**: 自动删除旧的进度数据库

### 手动清理
如果需要重新开始下载，可以手动删除进度文件：

```bash
# 删除进度数据库
rm output/*.progress.db

# 或者在配置中设置
resume = false
```

## 性能影响

### 内存
- 进度数据库使用 SQLite，内存占用极小
- 每个瓦片记录约 20 字节

### 磁盘
- 100万个瓦片的进度数据库约 20-50 MB
- 自动创建索引，查询速度快

### 速度
- 进度检查：< 1ms（数据库索引查询）
- 进度记录：异步写入，不影响下载速度

## 注意事项

1. **不同配置使用不同进度文件**
   - 进度文件名基于: `name-z{min}-{max}.progress.db`
   - 修改层级范围会创建新的进度文件

2. **跨机器恢复**
   - 可以复制整个输出目录到其他机器
   - 进度数据库和瓦片文件一起迁移即可恢复

3. **并发安全**
   - 进度数据库使用 SQLite 的 `INSERT OR IGNORE`
   - 支持多次下载同一瓦片不会冲突

## 故障排除

### 问题1: 进度数据库初始化失败
```
[WARN] Failed to setup progress database: ..., continuing without resume support
```
**解决**: 检查输出目录权限，确保程序有写入权限

### 问题2: 瓦片重复下载
**原因**: 可能同时禁用了 `resume` 和 `skipexisting`

**解决**: 至少启用其中一个
```toml
resume = true        # 推荐
# 或
skipexisting = true
```

### 问题3: 想要重新开始下载
**解决方案 1**: 禁用 resume
```toml
resume = false
```

**解决方案 2**: 删除进度文件
```bash
rm output/*.progress.db
```

## 示例完整配置

```toml
[output]
    format = "file"
    directory = "output"

[task]
    workers = 20
    savepipe = 20
    timedelay = 50
    skipexisting = true  # 跳过已存在的瓦片
    resume = true        # 支持任务恢复

[tm]
    name = "wuhan-google-satelite"
    min = 0
    max = 20
    format = "jpg"
    url = "http://mt0.google.com/vt/lyrs=s&x={x}&y={y}&z={z}"

[[lrs]]
    min = 1
    max = 20
    geojson = "./geojson/wuhan.geojson"
```

启用这些配置后，您可以随时中断下载，重新运行程序时会自动从上次停止的地方继续！
