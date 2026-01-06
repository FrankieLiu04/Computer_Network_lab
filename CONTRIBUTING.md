# 贡献指南 | Contributing Guide

感谢你对本项目的兴趣！以下是参与贡献的指南。

## 🌿 分支策略

我们采用简化的 Git Flow 模型：

| 分支 | 用途 | 保护级别 |
|------|------|----------|
| `main` | 稳定发布版本 | 受保护，需要 PR |
| `develop` | 开发集成分支 | 需要 PR |
| `feature/*` | 新功能开发 | 个人分支 |
| `bugfix/*` | Bug 修复 | 个人分支 |

### 分支命名规范

```
feature/添加文件压缩功能
feature/add-compression
bugfix/修复连接超时问题
bugfix/fix-connection-timeout
docs/更新README
```

## 📝 如何贡献

### 1. Fork & Clone

```bash
# Fork 本仓库后
git clone https://github.com/你的用户名/Computer_Network_lab.git
cd Computer_Network_lab
git remote add upstream https://github.com/FrankieLiu04/Computer_Network_lab.git
```

### 2. 创建功能分支

```bash
git checkout develop
git pull upstream develop
git checkout -b feature/你的功能名称
```

### 3. 开发 & 提交

```bash
# 进行开发...
git add .
git commit -m "feat: 添加新功能描述"
```

#### Commit 消息规范

我们使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `style` | 代码格式（不影响逻辑） |
| `refactor` | 重构 |
| `test` | 测试相关 |
| `chore` | 构建/工具变动 |

示例：
```
feat: 添加文件断点续传功能
fix: 修复大文件传输时的内存溢出问题
docs: 更新 Docker 部署文档
```

### 4. 推送 & 创建 PR

```bash
git push origin feature/你的功能名称
```

然后在 GitHub 上创建 Pull Request，目标分支选择 `develop`。

## ✅ PR 检查清单

提交 PR 前请确认：

- [ ] 代码能够正常编译
- [ ] 所有测试通过 (`ctest` 或 CI)
- [ ] 遵循代码风格规范
- [ ] 更新了相关文档（如需要）
- [ ] Commit 消息符合规范

## 🏗️ 本地开发环境

### 依赖

- CMake 3.16+
- C++17 兼容编译器
- Docker（可选，用于容器化测试）

### 构建

```bash
cd reliable-remote-backup-system
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### 测试

```bash
cd build
ctest --output-on-failure
```

## 💬 寻求帮助

- 有问题？创建 [Issue](https://github.com/FrankieLiu04/Computer_Network_lab/issues)
- 想讨论？使用 Issue 的 `question` 标签

## 📜 行为准则

请阅读我们的 [行为准则](CODE_OF_CONDUCT.md)，确保友好、包容的协作环境。
