# 故障排除指南

## 程序闪退问题

### 问题描述
运行程序时突然退出，没有错误提示。

### 可能的原因和解决方案

#### 1. GeoJSON 文件路径错误

**症状**: 程序立即退出，无任何输出

**检查**:
```toml
[[lrs]]
    geojson = "./geojson/nanjing.geojson"  # 相对路径
```

**解决方案**:
- 确保 `geojson` 目录存在
- 使用绝对路径：
  ```toml
  geojson = "E:/tiler/geojson/nanjing.geojson"
  ```
- 检查文件是否存在：
  ```bash
  ls geojson/nanjing.geojson
  ```

#### 2. GeoJSON 文件格式错误

**症状**: 解析 GeoJSON 时崩溃

**解决方案**:
启用日志文件查看详细错误：
```toml
[log]
    enable = true
    file = "tiler.log"
    level = "debug"
```

运行后检查 `tiler.log`：
```bash
tail -50 tiler.log
```

#### 3. 并发数过高导致内存不足

**症状**: 下载一段时间后闪退

**检查配置**:
```toml
[task]
    workers = 30    # 太高可能导致内存不足
    savepipe = 200  # 太高可能导致内存不足
```

**解决方案**:
降低并发数：
```toml
[task]
    workers = 5      # 减少并发线程
    savepipe = 10    # 减少缓冲区
```

#### 4. 磁盘空间不足

**症状**: 下载到一定数量后闪退

**检查**:
```bash
# Windows
dir output

# Linux/Mac
df -h
du -sh output/
```

**解决方案**:
- 清理磁盘空间
- 更改输出目录到空间更大的磁盘

#### 5. 网络错误未处理

**症状**: 随机闪退，通常在网络不稳定时

**解决方案**:
增加请求间隔：
```toml
[task]
    timedelay = 200  # 增加到200ms
```

#### 6. 数据库锁定问题

**症状**: 使用 mbtiles 格式时闪退

**解决方案**:
切换到文件格式：
```toml
[output]
    format = "file"  # 使用文件而不是mbtiles
```

或减少并发数：
```toml
[task]
    workers = 1      # 单线程避免数据库竞争
    savepipe = 1
```

## 调试步骤

### 第1步：启用详细日志

```toml
[log]
    enable = true
    file = "tiler-debug.log"
    level = "debug"
```

### 第2步：使用最小配置测试

```toml
[task]
    workers = 1
    savepipe = 1
    timedelay = 100

[tm]
    min = 10
    max = 10  # 只下载一个层级

[[lrs]]
    min = 10
    max = 10
    geojson = "./geojson/nanjing.geojson"
```

### 第3步：检查日志文件

```bash
# 查看最后的错误
tail -100 tiler-debug.log

# 搜索错误
grep -i error tiler-debug.log
grep -i fatal tiler-debug.log
```

### 第4步：逐步增加并发

如果最小配置工作正常，逐步增加：
1. 先增加 `max` 层级
2. 再增加 `workers`
3. 最后增加 `savepipe`

## 常见错误信息

### "unable to read file: no such file or directory"

**原因**: GeoJSON 文件不存在

**解决**:
```bash
# 列出所有 geojson 文件
ls geojson/

# 使用正确的文件名
[[lrs]]
    geojson = "./geojson/nanjing.geojson"
```

### "unable to unmarshal feature"

**原因**: GeoJSON 格式错误

**解决**:
在线验证 GeoJSON：https://geojson.io/

### "database is locked"

**原因**: 多个进程访问同一数据库

**解决**:
```bash
# 确保没有其他程序在运行
taskkill /F /IM tiler.exe

# 删除锁定的数据库
rm output/*.db-journal
```

### "permission denied"

**原因**: 没有写入权限

**解决**:
```bash
# Windows: 以管理员身份运行
# 或更改输出目录
[output]
    directory = "C:/Users/YourName/Downloads/tiler-output"
```

## 性能优化

### 推荐配置

#### 小范围下载（城市级别）
```toml
[task]
    workers = 10
    savepipe = 20
    timedelay = 50
```

#### 大范围下载（省级）
```toml
[task]
    workers = 5
    savepipe = 10
    timedelay = 100
```

#### 全球下载
```toml
[task]
    workers = 3
    savepipe = 5
    timedelay = 200
    resume = true  # 启用恢复很重要
```

## 测试 GeoJSON 文件

创建测试配置 `test.toml`：

```toml
[app]
    version = "v 0.1.0"
    title = "Test"

[log]
    enable = true
    file = "test.log"
    level = "debug"

[output]
    format = "file"
    directory = "output-test"

[task]
    workers = 1
    savepipe = 1
    timedelay = 100
    skipexisting = false
    resume = false

[tm]
    name = "test"
    min = 10
    max = 10
    format = "jpg"
    schema = "xyz"
    url = "http://mt0.google.com/vt/lyrs=s&x={x}&y={y}&z={z}"

[[lrs]]
    min = 10
    max = 10
    geojson = "./geojson/nanjing.geojson"
```

运行测试：
```bash
./tiler.exe -c test.toml
```

## 获取帮助

如果问题仍未解决：

1. **检查日志文件** - `tiler.log` 或 `tiler-debug.log`
2. **记录错误信息** - 完整的错误消息和日志
3. **提供配置文件** - `conf.toml` 的内容
4. **系统信息**:
   - 操作系统版本
   - Go 版本: `go version`
   - 可用内存
   - 磁盘空间

## 已验证的 GeoJSON 文件

以下文件已经过测试，可以正常使用：

✅ `global.geojson` - 全球范围
✅ `nanjing.geojson` - 南京市（11个区，38个瓦片@z10）
✅ 标准 FeatureCollection 格式
✅ OpenStreetMap Nominatim 格式

## 清理和重置

### 完全清理
```bash
# 删除所有输出
rm -rf output/
rm -rf output-test/

# 删除日志
rm *.log

# 删除进度数据库
rm output/*.progress.db
```

### 重新开始下载
```toml
[task]
    resume = false      # 禁用恢复
    skipexisting = false # 不跳过已存在
```

或手动删除进度文件：
```bash
rm output/*.progress.db
```
