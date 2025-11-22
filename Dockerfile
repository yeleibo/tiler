# 多阶段构建 Dockerfile
# 第一阶段：编译阶段
FROM golang:1.20-alpine AS builder

# 设置工作目录
WORKDIR /app

# 安装必要的构建工具和 CGO 依赖
RUN apk add --no-cache gcc musl-dev sqlite-dev git

# 设置 Go 环境变量
ENV CGO_ENABLED=1
ENV GOPROXY=https://goproxy.cn,direct
ENV GO111MODULE=on

# 复制所有源文件
COPY . .

# 清理并重新下载依赖
RUN go mod tidy && go mod download

# 编译
RUN go build -o tiler -ldflags="-s -w" .

# 第二阶段：运行阶段
FROM alpine:latest

# 安装运行时依赖
RUN apk add --no-cache ca-certificates sqlite-libs tzdata

# 设置时区（可选，根据需要调整）
ENV TZ=Asia/Shanghai

# 创建非 root 用户
RUN addgroup -g 1000 tiler && \
    adduser -D -u 1000 -G tiler tiler

# 设置工作目录
WORKDIR /app

# 从构建阶段复制编译好的二进制文件
COPY --from=builder /app/tiler .

# 创建必要的目录
RUN mkdir -p /app/output /app/geojson /app/data && \
    chown -R tiler:tiler /app

# 切换到非 root 用户
USER tiler

# 默认配置文件（可以通过挂载卷覆盖）
# COPY conf.toml .

# 暴露端口（如果应用有 web 服务的话）
# EXPOSE 8080

# 设置入口点
ENTRYPOINT ["./tiler"]

# 默认参数（可以在 docker run 时覆盖）
CMD ["-c", "/app/conf.toml"]


# docker buildx build --platform linux/amd64,linux/arm64  -t crpi-1arm6bubvql3ps3r.cn-beijing.personal.cr.aliyuncs.com/xfw-images/x-tiler:latest    --push .