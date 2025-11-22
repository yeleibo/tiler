# 日志文件功能说明

## 功能概述

程序现在支持将运行日志记录到文件，方便排查错误和追踪下载进度。

## 配置选项

在 `conf.toml` 中添加了 `[log]` 配置节：

```toml
[log]
    # 是否启用日志文件记录（true=启用, false=仅输出到控制台）
    enable = true
    # 日志文件路径（相对或绝对路径）
    file = "tiler.log"
    # 日志级别：debug, info, warn, error
    level = "info"
```

### 配置项说明

#### 1. `enable`
- **类型**: boolean
- **默认值**: `false`
- **说明**: 是否启用日志文件记录
  - `true`: 日志同时输出到控制台和文件
  - `false`: 日志仅输出到控制台

#### 2. `file`
- **类型**: string
- **默认值**: `"tiler.log"`
- **说明**: 日志文件的保存路径
  - 相对路径: 相对于程序运行目录，如 `"tiler.log"`
  - 绝对路径: 如 `"C:/logs/tiler.log"` 或 `"/var/log/tiler.log"`
  - 支持子目录: 如 `"logs/tiler.log"`（会自动创建目录）

#### 3. `level`
- **类型**: string
- **默认值**: `"info"`
- **可选值**:
  - `"debug"` - 调试级别，记录所有日志（最详细）
  - `"info"` - 信息级别，记录一般信息和更高级别
  - `"warn"` - 警告级别，记录警告和错误
  - `"error"` - 错误级别，仅记录错误（最简洁）

### 日志级别对比

| 级别 | Debug | Info | Warn | Error |
|------|-------|------|------|-------|
| **debug日志** | ✓ | ✗ | ✗ | ✗ |
| **info日志** | ✓ | ✓ | ✗ | ✗ |
| **warn日志** | ✓ | ✓ | ✓ | ✗ |
| **error日志** | ✓ | ✓ | ✓ | ✓ |

## 使用示例

### 场景 1: 生产环境（推荐）

```toml
[log]
    enable = true
    file = "tiler.log"
    level = "info"
```

**效果**:
- 记录信息、警告和错误到文件
- 同时在控制台显示
- 日志文件大小适中

### 场景 2: 调试模式

```toml
[log]
    enable = true
    file = "tiler-debug.log"
    level = "debug"
```

**效果**:
- 记录所有详细日志，包括每个瓦片的下载详情
- 适合排查问题
- 日志文件较大

### 场景 3: 仅记录错误

```toml
[log]
    enable = true
    file = "tiler-errors.log"
    level = "error"
```

**效果**:
- 仅记录错误信息
- 日志文件最小
- 适合只关注错误的场景

### 场景 4: 禁用日志文件

```toml
[log]
    enable = false
```

**效果**:
- 不创建日志文件
- 所有日志仅显示在控制台
- 适合临时运行

## 日志文件格式

日志文件使用以下格式：

```
2025-01-15 10:30:45.123 [INFO ] Progress tracking enabled, task can be resumed after interruption
2025-01-15 10:30:45.456 [INFO ] Zoom: 10, tiles: 1024
2025-01-15 10:30:46.789 [INFO ] tile(z:10, x:123, y:456), 150ms, 45.2 kb, http://...
2025-01-15 10:30:47.012 [WARN ] Failed to mark tile as downloaded: database locked
2025-01-15 10:30:48.345 [ERROR] fetch tile error, status code: 404
```

### 日志字段说明

- **时间戳**: `2025-01-15 10:30:45.123` - 精确到毫秒
- **级别**: `[INFO]`, `[WARN]`, `[ERROR]`, `[DEBUG]`
- **消息**: 具体的日志内容

## 日志文件管理

### 追加模式
日志文件使用**追加模式**：
- 每次运行不会覆盖旧日志
- 新日志追加到文件末尾
- 方便查看历史记录

### 手动清理
日志文件不会自动清理，需要手动管理：

```bash
# Windows
del tiler.log

# Linux/Mac
rm tiler.log
```

### 日志轮转（推荐）
对于长期运行的任务，建议定期清理或归档日志：

```bash
# 按日期归档
mv tiler.log tiler-2025-01-15.log

# 或使用日期命名
[log]
    file = "tiler-2025-01-15.log"
```

## 典型日志内容

### 启动日志
```
2025-01-15 10:30:45.000 [INFO ] Logging to file: tiler.log
2025-01-15 10:30:45.100 [INFO ] zoom: 10, tiles: 1024
2025-01-15 10:30:45.200 [INFO ] zoom: 11, tiles: 4096
2025-01-15 10:30:45.300 [INFO ] Progress tracking enabled, task can be resumed after interruption
```

### 下载日志（info 级别）
```
2025-01-15 10:30:46.000 [INFO ] tile(z:10, x:123, y:456), 150ms, 45.2 kb, http://...
2025-01-15 10:30:46.200 [INFO ] tile(z:10, x:124, y:456), 180ms, 48.1 kb, http://...
```

### 跳过日志（debug 级别）
```
2025-01-15 10:30:46.500 [DEBUG] tile(z:10, x:125, y:456) already downloaded (from progress), skipping...
2025-01-15 10:30:46.501 [DEBUG] tile(z:10, x:126, y:456) already exists, skipping...
```

### 警告日志
```
2025-01-15 10:30:47.000 [WARN ] Failed to mark tile as downloaded: database is locked
2025-01-15 10:30:47.100 [WARN ] save tile to mbtiles db error ~ UNIQUE constraint failed
```

### 错误日志
```
2025-01-15 10:30:48.000 [ERROR] fetch tile error, status code: 404
2025-01-15 10:30:48.100 [ERROR] read tile error ~ EOF
2025-01-15 10:30:48.200 [ERROR] save tile file error ~ permission denied
```

### 完成日志
```
2025-01-15 10:45:30.000 [INFO ] Task abc123 Zoom 10 finished ~
2025-01-15 10:45:30.100 [INFO ] Task abc123 finished ~
2025-01-15 10:45:30.200 [INFO ] 895.234s finished...
```

## 双输出特性

启用日志文件后，日志会**同时**输出到：
1. **控制台** - 实时查看运行状态
2. **日志文件** - 持久化保存，方便事后分析

这意味着：
- 你可以在控制台实时监控
- 关闭程序后仍可查看完整日志
- 不影响控制台的颜色显示

## 性能影响

### 内存
- 日志缓冲区极小（< 1 MB）
- 不会占用大量内存

### 磁盘
- 日志文件大小取决于级别和下载数量
- **debug**: 约 1-5 KB/瓦片
- **info**: 约 200-500 B/瓦片
- **warn**: 仅警告和错误，通常很小
- **error**: 仅错误，最小

### 示例文件大小
下载 10,000 个瓦片：
- **debug 级别**: 约 10-50 MB
- **info 级别**: 约 2-5 MB
- **warn 级别**: < 1 MB
- **error 级别**: < 100 KB

### 速度
- 写入日志文件不会显著影响下载速度
- 日志写入是异步的，不阻塞下载线程

## 故障排除

### 问题 1: 日志文件未创建
**原因**: 可能是权限问题或路径不存在

**解决**:
```toml
# 使用绝对路径
file = "C:/logs/tiler.log"

# 或确保程序有写入权限
```

### 问题 2: 日志级别不生效
**检查**: 确保配置文件格式正确
```toml
[log]
    level = "debug"  # 注意引号
```

### 问题 3: 日志文件过大
**解决**: 调整日志级别或定期清理
```toml
[log]
    level = "warn"  # 减少日志量
```

## 完整配置示例

```toml
[app]
    version = "v 0.1.0"
    title = "MapCloud Tiler"

[log]
    enable = true          # 启用日志文件
    file = "tiler.log"     # 日志文件名
    level = "info"         # 信息级别

[output]
    format = "file"
    directory = "output"

[task]
    workers = 20
    savepipe = 20
    timedelay = 50
    skipexisting = true
    resume = true

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

## 建议配置

### 日常使用
```toml
[log]
    enable = true
    file = "tiler.log"
    level = "info"
```

### 排查问题
```toml
[log]
    enable = true
    file = "tiler-debug.log"
    level = "debug"
```

### 生产环境
```toml
[log]
    enable = true
    file = "logs/tiler-production.log"
    level = "warn"
```

启用日志文件后，所有运行错误、警告和信息都会被记录，方便事后分析和问题排查！
