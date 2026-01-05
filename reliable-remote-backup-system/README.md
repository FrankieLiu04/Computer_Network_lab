# Reliable Remote Backup System

[![CI](https://github.com/YOUR_USERNAME/reliable-remote-backup-system/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/reliable-remote-backup-system/actions/workflows/ci.yml)

> 基于 UDP 命令通道 + TCP 数据通道的远程备份系统，实现文件列表/上传/删除/重命名/关停；支持 Docker 一键部署。

## 🎯 项目概述

本项目是一个可靠的远程文件备份系统，具有以下特性：

- **UDP/TCP Socket 编程**：命令通道使用 UDP，大文件传输使用 TCP
- **可靠性设计**：文件名校验、超时重试、显式握手
- **可观测性**：结构化日志（spdlog）
- **工程化**：CMake 构建、单元测试（GoogleTest）、Docker 支持
- **CI/CD**：GitHub Actions 自动化构建和测试

## 📁 项目结构

```
reliable-remote-backup-system/
├── include/                    # 头文件
│   ├── client/                 # 客户端头文件
│   ├── common/                 # 公共模块头文件
│   ├── protocol/               # 协议定义
│   └── server/                 # 服务端头文件
├── src/                        # 源代码
│   ├── client/                 # 客户端实现
│   ├── common/                 # 公共模块实现
│   └── server/                 # 服务端实现
├── tests/                      # 单元测试
├── .github/workflows/          # GitHub Actions CI
├── CMakeLists.txt              # CMake 构建配置
├── Dockerfile                  # 服务端 Docker 镜像
├── Dockerfile.client           # 客户端 Docker 镜像
├── docker-compose.yml          # Docker Compose 配置
└── README.md                   # 本文件
```

## 🚀 快速开始

### 前置要求

- CMake 3.16+
- C++17 兼容编译器 (GCC 8+, Clang 8+, MSVC 2019+)
- Git

### 本地构建

```bash
# 克隆项目
cd Computer_Network_lab/reliable-remote-backup-system

# 创建构建目录
mkdir build && cd build

# 配置和构建
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel

# 运行测试
ctest --output-on-failure
```

### 运行

**启动服务端：**
```bash
./backup-server -port 35887
```

**启动客户端：**
```bash
./backup-client -port 35887
# 或连接远程服务器
./backup-client -address 192.168.1.100 -port 35887
```

### Docker 部署

```bash
# 一键启动
docker compose up -d

# 查看日志
docker compose logs -f

# 停止服务
docker compose down
```

## 📖 命令说明

| 命令 | 语法 | 描述 |
|------|------|------|
| `ls` | `ls` | 列出服务器备份目录中的所有文件 |
| `send` | `send <filename>` | 上传本地文件到服务器 |
| `remove` | `remove <filename>` | 删除服务器上的文件 |
| `rename` | `rename <old> <new>` | 重命名服务器上的文件 |
| `shutdown` | `shutdown` | 关闭服务器 |
| `quit` | `quit` | 退出客户端 |

### 使用示例

```
$ ls
 - server backup folder is empty.

$ send myfile.txt
 - filesize:1234
 - file transmission is completed.

$ ls
Files on server:
 - myfile.txt

$ rename myfile.txt newname.txt
 -file has been renamed.

$ remove newname.txt
 - file is removed.

$ quit
Exiting client
```

## 🔧 命令行参数

### Server

```
backup-server [-port <udp_port>] [-backup <dir>] [-log]

参数:
  -port <port>     UDP 监听端口 (默认: 自动分配)
  -backup <dir>    备份目录 (默认: backup)
  -log             启用文件日志
  -help            显示帮助信息
```

### Client

```
backup-client [-address <server_host>] -port <udp_port>

参数:
  -address <host>  服务器地址 (默认: 127.0.0.1)
  -port <port>     服务器 UDP 端口 (必需)
  -log             启用文件日志
  -help            显示帮助信息
```

## 🛡️ 安全特性

- **文件名校验**：防止路径遍历攻击 (`../`)
- **禁止绝对路径**：只允许相对文件名
- **保留名称检测**：阻止 Windows 保留文件名 (CON, PRN, etc.)
- **非法字符过滤**：禁止 `<>:"|?*` 等特殊字符

## 🧪 测试

```bash
# 运行所有单元测试
cd build
ctest --output-on-failure

# 运行特定测试
./unit_tests --gtest_filter=FilenameValidationTest.*
```

## 📊 错误码

| 范围 | 类别 | 示例 |
|------|------|------|
| 0 | 成功 | SUCCESS |
| 1-99 | 通用错误 | TIMEOUT, CANCELLED |
| 100-199 | 文件错误 | FILE_NOT_FOUND, FILE_EXISTS |
| 200-299 | 目录错误 | DIRECTORY_NOT_FOUND |
| 300-399 | 网络错误 | SOCKET_CREATE_ERROR |
| 400-499 | 协议错误 | INVALID_COMMAND |
| 500-599 | 服务器错误 | SERVER_SHUTDOWN |

## 🐳 Docker 端口说明

| 端口 | 协议 | 用途 |
|------|------|------|
| 35887 | UDP | 命令通道 |
| 40000-40010 | TCP | 文件传输 |

## 📈 项目演进

### 阶段一 (当前) - 工程化底座
- [x] C++17 + CMake 构建
- [x] spdlog 日志
- [x] GoogleTest 单元测试
- [x] Docker 支持
- [x] GitHub Actions CI
- [x] 文件名安全校验
- [x] rename 时序修复（显式握手）

### 阶段二 (计划中) - Web 管理台
- [ ] HTTP API (`/api/files`, `/api/metrics`)
- [ ] Web 文件管理界面
- [ ] 性能指标展示

### 阶段三 (计划中) - 高级特性
- [ ] Prometheus 指标导出
- [ ] 并发传输支持
- [ ] 压测与扰动测试

## 📄 协议格式

### 命令消息 (CmdMsg)

```cpp
struct CmdMsg {
    uint8_t cmd;              // 命令类型
    char filename[128];       // 文件名
    uint32_t size;            // 文件大小或端口
    uint16_t port;            // TCP 端口
    uint16_t error;           // 错误码
};
```

### 数据消息 (DataMsg)

```cpp
struct DataMsg {
    char data[3000];          // 数据内容
};
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📜 License

MIT License

---

**作者**: Frankie  
**项目**: Reliable Remote Backup System
