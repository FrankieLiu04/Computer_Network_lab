# 本地运行指南

## 方案一：本机构建和运行（推荐用于开发）

### 前置要求

**Windows:**
- Visual Studio 2019+ 或 MinGW-w64
- CMake 3.16+
- Git

**Linux/macOS:**
- GCC 8+ 或 Clang 8+
- CMake 3.16+
- Git

### 步骤 1: 克隆项目

```bash
cd Computer_Network_lab
git clone <repository-url>
cd reliable-remote-backup-system
```

或如果已克隆，更新到最新代码：

```bash
git pull origin main
```

### 步骤 2: 创建构建目录并配置

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### 步骤 3: 编译

```bash
# Linux/macOS
cmake --build . --parallel $(nproc)

# Windows (PowerShell)
cmake --build . --parallel $env:NUMBER_OF_PROCESSORS
```

或使用系统编译命令：

```bash
# Linux/macOS
make -j$(nproc)

# Windows (Visual Studio)
msbuild reliable-remote-backup-system.sln /p:Configuration=Release /m
```

### 步骤 4: 运行单元测试（可选）

```bash
# Linux/macOS
ctest --output-on-failure

# Windows
ctest --output-on-failure -C Release
```

### 步骤 5: 启动服务器

```bash
# Linux/macOS
./backup-server -port 35887 -http 8080

# Windows
.\backup-server.exe -port 35887 -http 8080
```

输出应该看起来像：

```
[2026-01-06 10:30:45.123] [server] [info] Starting backup server...
[2026-01-06 10:30:45.124] [server] [info] Server initialized on UDP port 35887
[2026-01-06 10:30:45.125] [server] [info] HTTP management server started on port 8080
HTTP management server @ http://0.0.0.0:8080
**************************************************************
Reliable Remote Backup System - Server v1.0
**************************************************************
Waiting UDP command @ port number: 35887
```

### 步骤 6: 访问 Web 管理台

打开浏览器访问：**http://localhost:8080**

你应该看到：
- 📊 请求统计（总请求数、HTTP 请求、上传数、错误数）
- 📈 传输统计（收发字节、会话数、失败数）
- 📁 文件列表（空的，等待上传）
- 📤 上传区域（拖拽或点击选择文件）

### 步骤 7: 测试 Web 功能

1. **上传文件**
   - 点击上传区域或拖拽文件
   - 选择本地文件上传

2. **查看文件列表**
   - 文件列表会自动刷新
   - 显示文件名和大小

3. **重命名文件**
   - 点击文件行的"Rename"按钮
   - 输入新名字

4. **删除文件**
   - 点击文件行的"Delete"按钮
   - 确认删除

5. **查看指标**
   - 指标会实时更新（每 5 秒刷新）
   - 显示上传数、错误数、字节数等

### 步骤 8: 测试 CLI 客户端（可选）

在另一个终端启动客户端：

```bash
# Linux/macOS
./backup-client -port 35887

# Windows
.\backup-client.exe -port 35887
```

测试命令：

```
ls
send myfile.txt
ls
rename myfile.txt newname.txt
remove newname.txt
quit
```

---

## 方案二：Docker Compose 运行（推荐用于演示）

### 前置要求

- Docker 20.10+
- Docker Compose 1.29+

### 步骤 1: 进入项目目录

```bash
cd reliable-remote-backup-system
```

### 步骤 2: 一键启动

```bash
docker compose up -d
```

等待 30 秒让服务启动。可以查看日志：

```bash
docker compose logs -f backup-server
```

### 步骤 3: 访问 Web 管理台

打开浏览器访问：**http://localhost:8080**

### 步骤 4: 运行集成测试（可选）

```bash
bash scripts/integration_test.sh
```

### 步骤 5: 停止服务

```bash
docker compose down
```

清空数据卷（如需）：

```bash
docker compose down -v
```

---

## 方案三：使用 Docker CLI 逐个启动

### 构建镜像

```bash
# 构建服务端镜像
docker build -f Dockerfile -t backup-server:latest .

# 构建客户端镜像（可选）
docker build -f Dockerfile.client -t backup-client:latest .
```

### 创建网络

```bash
docker network create backup-net
```

### 启动服务端

```bash
docker run -d \
  --name backup-server \
  --network backup-net \
  -p 35887:35887/udp \
  -p 40000-40010:40000-40010/tcp \
  -p 8080:8080/tcp \
  -v backup-data:/app/backup \
  backup-server:latest
```

### 查看日志

```bash
docker logs -f backup-server
```

### 测试连接

```bash
# 检查健康状态
curl http://localhost:8080/healthz

# 获取文件列表
curl http://localhost:8080/api/files

# 获取指标
curl http://localhost:8080/api/metrics | jq
```

### 上传文件测试

```bash
# 创建测试文件
echo "Hello, Backup System!" > test.txt

# 上传
curl -F "file=@test.txt" http://localhost:8080/api/upload

# 查看文件列表
curl http://localhost:8080/api/files | jq
```

### 停止服务

```bash
docker stop backup-server
docker rm backup-server
```

---

## 快速诊断

### 健康检查

```bash
# 检查 HTTP 服务
curl -v http://localhost:8080/healthz

# 检查 UDP 端口（Linux）
nc -u -z localhost 35887 && echo "UDP 35887 open" || echo "UDP 35887 closed"
```

### 查看日志

**本机运行:**
```bash
# 启用文件日志
./backup-server -port 35887 -http 8080 -log

# 查看日志
tail -f logs/server.log
```

**Docker:**
```bash
docker logs -f backup-server
```

### API 测试

```bash
# 使用 curl 测试所有 API
BASE_URL="http://localhost:8080"

# 1. 健康检查
curl $BASE_URL/healthz | jq

# 2. 获取文件列表
curl $BASE_URL/api/files | jq

# 3. 上传文件
curl -F "file=@/path/to/file" $BASE_URL/api/upload | jq

# 4. 重命名
curl -X POST -H "Content-Type: application/json" \
  -d '{"oldName":"old.txt","newName":"new.txt"}' \
  $BASE_URL/api/rename | jq

# 5. 删除
curl -X DELETE $BASE_URL/api/files/new.txt | jq

# 6. 获取指标
curl $BASE_URL/api/metrics | jq
```

---

## 常见问题

### Q: 端口被占用怎么办？

**Windows (PowerShell):**
```powershell
# 查找占用 8080 端口的进程
Get-NetTCPConnection -LocalPort 8080

# 杀死进程
Stop-Process -Id <PID> -Force
```

**Linux/macOS:**
```bash
# 查找占用 8080 端口的进程
lsof -i :8080

# 杀死进程
kill -9 <PID>
```

### Q: 如何修改 HTTP 端口？

**本机运行:**
```bash
./backup-server -port 35887 -http 9000
```

**Docker Compose:**
修改 `docker-compose.yml` 中的端口映射：
```yaml
ports:
  - "9000:8080/tcp"  # 将 9000 映射到容器的 8080
```

### Q: 如何重置数据？

**Docker Compose:**
```bash
docker compose down -v  # 删除卷数据
docker compose up -d    # 重新启动
```

**本机运行:**
```bash
rm -rf backup/  # 删除备份目录
```

### Q: 如何查看所有请求日志？

**启用文件日志:**
```bash
./backup-server -port 35887 -http 8080 -log
tail -f logs/server.log
```

---

## 性能测试

### 简单压测脚本

```bash
#!/bin/bash
# 创建测试文件
dd if=/dev/urandom of=test_1mb.bin bs=1M count=1
dd if=/dev/urandom of=test_10mb.bin bs=1M count=10

# 上传测试
echo "上传 1MB 文件..."
time curl -F "file=@test_1mb.bin" http://localhost:8080/api/upload

echo "上传 10MB 文件..."
time curl -F "file=@test_10mb.bin" http://localhost:8080/api/upload

# 查看指标
curl http://localhost:8080/api/metrics | jq '.transfer'

# 清理
rm test_*.bin
```

### 并发上传测试

```bash
#!/bin/bash
# 创建多个测试文件并并发上传
for i in {1..5}; do
  echo "Concurrent upload $i" > file_$i.txt
  (curl -F "file=@file_$i.txt" http://localhost:8080/api/upload &)
done
wait

# 查看文件列表
curl http://localhost:8080/api/files | jq '.count'

# 清理
rm file_*.txt
```

---

## 下一步

运行成功后，你可以：

1. **探索 Web UI**：上传、删除、重命名文件，观察指标变化
2. **测试 API**：使用 curl 或 Postman 测试各个 HTTP 端点
3. **运行集成测试**：`bash scripts/integration_test.sh`
4. **查看源代码**：理解 HTTP 服务器和 Metrics 的实现
5. **开发新功能**：基于现有代码添加新的 API 端点或 Web 功能

有问题？检查：
- 日志文件（`logs/server.log`）
- HTTP 响应（使用 `-v` 选项的 curl）
- Docker 日志（`docker logs backup-server`）
