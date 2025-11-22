# Docker 使用说明

## 快速开始

### 1. 构建镜像

```bash
docker build -t tiler:latest .
```

### 2. 使用 Docker Compose 运行（推荐）

```bash
# 启动容器
docker-compose up -d

# 查看日志
docker-compose logs -f

# 停止容器
docker-compose down
```

### 3. 直接使用 Docker 运行

```bash
docker run -d \
  --name tiler \
  -v $(pwd)/conf.toml:/app/conf.toml:ro \
  -v $(pwd)/geojson:/app/geojson:ro \
  -v $(pwd)/output:/app/output \
  -v $(pwd)/data:/app/data \
  -v $(pwd)/tiler.log:/app/tiler.log \
  tiler:latest
```

## 目录说明

### 需要挂载的目录

- **conf.toml**: 配置文件（只读）
- **geojson/**: GeoJSON 文件目录（只读）
- **output/**: 瓦片输出目录（读写）
- **data/**: 数据目录，用于存储任务恢复数据库等（读写）
- **tiler.log**: 日志文件（读写，可选）

### 目录结构示例

```
tiler/
├── Dockerfile
├── docker-compose.yml
├── conf.toml
├── geojson/
│   └── 几内亚.geojson
├── output/          # 自动创建
├── data/            # 自动创建
└── tiler.log        # 自动创建
```

## 常用命令

### 构建和运行

```bash
# 构建镜像
docker build -t tiler:latest .

# 使用 docker-compose 启动
docker-compose up -d

# 查看容器状态
docker-compose ps

# 查看实时日志
docker-compose logs -f tiler

# 停止并移除容器
docker-compose down

# 停止但不移除容器
docker-compose stop
```

### 进入容器调试

```bash
# 进入运行中的容器
docker exec -it tiler sh

# 查看容器内文件
docker exec tiler ls -la /app
```

### 清理

```bash
# 删除容器
docker-compose down

# 删除镜像
docker rmi tiler:latest

# 清理未使用的镜像和容器
docker system prune -a
```

## 配置说明

### 修改配置文件

在启动容器前，请确保 `conf.toml` 已正确配置：

1. **输出目录**: 默认为 `/app/output`（容器内路径）
2. **GeoJSON 路径**: 需要使用容器内路径 `/app/geojson/xxx.geojson`
3. **日志文件**: 默认为 `/app/tiler.log`

示例配置（conf.toml）：

```toml
[output]
  format = "file"
  directory = "/app/output"  # 容器内路径

[[lrs]]
  min = 1
  max = 18
  geojson = "/app/geojson/几内亚.geojson"  # 容器内路径
```

### Windows 下的路径挂载

如果在 Windows 上使用，需要修改 docker-compose.yml 中的路径：

```yaml
volumes:
  - ./conf.toml:/app/conf.toml:ro
  - ./geojson:/app/geojson:ro
  - ./output:/app/output
  - ./data:/app/data
```

或使用绝对路径：

```yaml
volumes:
  - E:/tiler/conf.toml:/app/conf.toml:ro
  - E:/tiler/geojson:/app/geojson:ro
  - E:/tiler/output:/app/output
  - E:/tiler/data:/app/data
```

## 资源限制

可以在 docker-compose.yml 中取消注释以下部分来限制资源使用：

```yaml
deploy:
  resources:
    limits:
      cpus: '2'        # 最多使用 2 个 CPU 核心
      memory: 2G       # 最多使用 2GB 内存
    reservations:
      cpus: '1'        # 至少保留 1 个 CPU 核心
      memory: 1G       # 至少保留 1GB 内存
```

## 任务恢复功能

项目支持任务恢复功能，为了确保任务进度不丢失：

1. 确保挂载 `data` 目录用于存储恢复数据库
2. 在 `conf.toml` 中设置 `task.resume = true`
3. 容器重启后会自动从上次中断的地方继续下载

## 多实例运行

如果需要同时运行多个下载任务，可以复制配置文件并修改 docker-compose.yml：

```yaml
version: '3.8'

services:
  tiler-task1:
    build: .
    volumes:
      - ./conf-task1.toml:/app/conf.toml:ro
      - ./geojson:/app/geojson:ro
      - ./output-task1:/app/output
      - ./data-task1:/app/data

  tiler-task2:
    build: .
    volumes:
      - ./conf-task2.toml:/app/conf.toml:ro
      - ./geojson:/app/geojson:ro
      - ./output-task2:/app/output
      - ./data-task2:/app/data
```

## 故障排查

### 权限问题

如果遇到权限问题，可以修改 Dockerfile 中的用户 ID：

```dockerfile
RUN addgroup -g 1000 tiler && \
    adduser -D -u 1000 -G tiler tiler
```

将 1000 改为你的用户 ID（使用 `id -u` 查看）。

### 查看日志

```bash
# 查看容器日志
docker-compose logs tiler

# 实时查看日志
docker-compose logs -f tiler

# 查看容器内的日志文件
docker exec tiler cat /app/tiler.log
```

### 网络问题

如果下载失败，可能是网络问题。可以在 docker-compose.yml 中配置代理：

```yaml
environment:
  - HTTP_PROXY=http://your-proxy:port
  - HTTPS_PROXY=http://your-proxy:port
```

## 注意事项

1. **CGO 依赖**: 项目使用 SQLite，需要 CGO 支持，已在 Dockerfile 中配置
2. **时区设置**: 默认使用 Asia/Shanghai 时区，可在 docker-compose.yml 中修改
3. **数据持久化**: 确保 output 和 data 目录正确挂载，避免数据丢失
4. **配置文件**: conf.toml 需要在启动前准备好，容器启动后修改需要重启容器
