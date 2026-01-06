# Docker Compose 快速启动指南

## 最简单的方式（推荐演示使用）

### 1️⃣ 一键启动

```bash
docker compose up -d
```

等待 10-30 秒让服务启动。

### 2️⃣ 打开 Web 管理台

在浏览器中访问：**http://localhost:8080**

你会看到：
- 📊 实时指标（请求数、错误数、吞吐量）
- 📁 文件列表
- 📤 拖拽上传区域

### 3️⃣ 测试功能

**上传文件：**
```bash
# 创建测试文件
echo "Hello, Backup System!" > test.txt

# 上传（可以在 Web UI 中拖拽，也可以用 curl）
curl -F "file=@test.txt" http://localhost:8080/api/upload
```

**查看文件列表：**
```bash
curl http://localhost:8080/api/files | jq
```

**重命名文件：**
```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"oldName":"test.txt","newName":"test_renamed.txt"}' \
  http://localhost:8080/api/rename | jq
```

**删除文件：**
```bash
curl -X DELETE http://localhost:8080/api/files/test_renamed.txt
```

**查看指标：**
```bash
curl http://localhost:8080/api/metrics | jq
```

### 4️⃣ 查看日志

```bash
# 查看服务端日志
docker compose logs -f backup-server

# 查看所有服务日志
docker compose logs -f
```

### 5️⃣ 停止服务

```bash
# 停止但保留数据
docker compose down

# 完全清理（包括数据卷）
docker compose down -v
```

---

## 运行集成测试

```bash
# 启动服务（如果还没启动）
docker compose up -d

# 运行集成测试
bash scripts/integration_test.sh
```

预期输出：
```
============================================
HTTP API Integration Tests
Base URL: http://localhost:8080
============================================
[INFO] Waiting for server...
[INFO] Server is ready
[INFO] Testing health check endpoint...
[PASS] Health check: status=ok
[INFO] Testing get files (may be empty)...
[PASS] Get files: count=0
...
============================================
Test Results: 9 passed, 0 failed
============================================
```

---

## 常见操作

### 查看容器状态

```bash
docker compose ps
```

输出应该显示 `backup-server` 容器状态为 `healthy`（或 `up`）。

### 进入容器 Shell

```bash
docker compose exec backup-server bash
```

### 查看备份文件

```bash
# 检查服务端的 /app/backup 目录
docker compose exec backup-server ls -la /app/backup

# 或者访问 volume
docker volume inspect reliable-backup-data
```

### 清除所有数据

```bash
# 删除数据卷
docker volume rm reliable-backup-data

# 下次启动时会创建新的空卷
docker compose up -d
```

### 重新构建镜像

```bash
# 如果源代码有改动，需要重新构建
docker compose build --no-cache

# 然后重启
docker compose up -d
```

---

## 性能测试

### 简单的吞吐量测试

```bash
# 创建 10MB 测试文件
dd if=/dev/urandom of=test_10mb.bin bs=1M count=10

# 上传并测量时间
time curl -F "file=@test_10mb.bin" http://localhost:8080/api/upload

# 查看指标
curl http://localhost:8080/api/metrics | jq '.transfer'

# 清理
rm test_10mb.bin
```

预期输出类似：
```
{
  "bytes_received": 10485760,
  "bytes_sent": 0,
  "send_sessions": 1,
  "send_failures": 0
}
```

### 并发上传测试

```bash
#!/bin/bash
# 并发上传 5 个文件
for i in {1..5}; do
  echo "File $i content" > file_$i.txt
  curl -F "file=@file_$i.txt" http://localhost:8080/api/upload &
done
wait

# 验证所有文件都上传成功
curl http://localhost:8080/api/files | jq '.count'

# 清理
rm file_*.txt
```

---

## 故障排除

### 端口已被占用

```bash
# 找到占用 8080 的进程（Linux/macOS）
lsof -i :8080

# 或使用 Docker 映射到不同端口
# 修改 docker-compose.yml:
# ports:
#   - "9000:8080/tcp"

docker compose up -d
# 然后访问 http://localhost:9000
```

### 健康检查失败

```bash
# 检查容器日志
docker compose logs backup-server

# 确保容器能访问 curl 命令
docker compose exec backup-server curl -f http://localhost:8080/healthz

# 如果 curl 不可用，检查 Dockerfile
# 确保 apt-get install -y curl 被执行
```

### 无法连接到 API

```bash
# 检查容器是否运行
docker compose ps

# 检查端口映射
docker compose port backup-server 8080

# 测试基本连接
curl -v http://localhost:8080/healthz
```

### 文件上传失败

```bash
# 检查备份目录权限
docker compose exec backup-server ls -la /app/backup

# 查看 backup 目录挂载
docker inspect reliable-backup-data

# 查看详细错误日志
docker compose logs -f backup-server | grep -i error
```

---

## 高级用法

### 自定义环境变量

创建 `.env` 文件：

```bash
# .env
TZ=Asia/Hong_Kong
BACKUP_DIR=/app/backup
HTTP_PORT=8080
```

然后启动：

```bash
docker compose --env-file .env up -d
```

### 持久化日志

修改 `docker-compose.yml`：

```yaml
backup-server:
  ...
  volumes:
    - backup-data:/app/backup
    - ./logs:/app/logs
```

启动时启用日志：

```bash
docker compose up -d
# 修改启动命令
docker compose exec backup-server ./backup-server -port 35887 -http 8080 -log
```

然后查看本地 `logs/` 目录。

### 使用特定版本

```bash
# 查看可用镜像标签
docker image ls

# 使用特定版本
docker compose -f docker-compose.yml up -d
```

---

## 完整 API 参考

所有请求都使用 `http://localhost:8080`

| 端点 | 方法 | 描述 | 示例 |
|------|------|------|------|
| `/healthz` | GET | 健康检查 | `curl http://localhost:8080/healthz \| jq` |
| `/api/files` | GET | 获取文件列表 | `curl http://localhost:8080/api/files \| jq` |
| `/api/upload` | POST | 上传文件 | `curl -F "file=@test.txt" http://localhost:8080/api/upload` |
| `/api/files/{name}` | DELETE | 删除文件 | `curl -X DELETE http://localhost:8080/api/files/test.txt` |
| `/api/rename` | POST | 重命名文件 | `curl -X POST -H "Content-Type: application/json" -d '{"oldName":"a","newName":"b"}' http://localhost:8080/api/rename` |
| `/api/metrics` | GET | 获取指标 | `curl http://localhost:8080/api/metrics \| jq` |

---

## 下一步

✅ **已完成：**
- 本地构建和运行
- Web 管理台
- HTTP API
- Docker 容器化

🚀 **可以尝试的：**
1. 修改源代码并重新构建
2. 添加新的 HTTP 端点
3. 扩展 Web UI 功能
4. 运行压力测试
5. 部署到云服务

有问题？查看：
- 主文档：[README.md](../README.md)
- 本地运行指南：[LOCAL_RUN_GUIDE.md](LOCAL_RUN_GUIDE.md)
- 项目源代码：[src/](../src/)
