# 可靠远程备份系统（Remote Backup System）整合改写计划

> 目标：把 Lab1（可靠传输协议：SR/GBN）与 Lab2（UDP/TCP 远程备份系统）整合为一个更贴近“后台开发”的可演示项目，并补齐：
> - **Web 管理界面**（文件管理 + 性能监控）
> - **可观测性**（metrics + 日志 + health check）
> - **Docker 部署**（一键启动与可复现）
> - **测试与发布流程**（CI/CD，例如 GitHub Actions）
>
> 原则：尽量复用现有 Lab2 的协议与实现，不大改 CLI 交互；新增内容优先体现“稳定性、可观测、可部署、可验证”。

---

## 0. 项目定位（面向腾讯后台开发实习）

**项目名（建议）**：Reliable Remote Backup System（高可用远程备份/文件传输系统）

**核心卖点（简历关键词）**
- TCP/UDP Socket 编程、网络协议与异常处理
- 可用性与稳定性：校验、超时、重试、握手、错误码
- 可观测性：吞吐、延迟、重传/失败率、日志
- 可部署性：Docker 化，一键启动 demo
- 问题定位：跨机器时序/竞态问题（Lab2 rename 的真实网络差异）

**对外描述（1-2句）**
- 基于 UDP 命令通道 + TCP 数据通道的远程备份系统，实现文件列表/上传/删除/重命名/关停；增加 Web 管理台展示文件与性能指标，并支持 Docker 一键部署。

---

## 1. 现状盘点（基于现有 Lab1/Lab2）

### 1.1 Lab1（`lab1/`）

Lab1 的定位是“可靠传输机制的实现与性能对比”，能直接支撑你在简历/面试中讲清楚：你如何思考丢包/损坏、重传、吞吐与稳定性。

**已有实现（按 SR/GBN 两套协议）**
- SR（Selective Repeat）：
  - 发送/接收窗口管理（允许乱序接收）
  - 接收方缓存乱序分组，并在条件满足时按序交付
  - 每个已发送分组独立计时器，超时仅重传对应分组
  - ACK 粒度到分组
- GBN（Go-Back-N）：
  - 累积 ACK（ack 表示“到某序号为止均已收到”）
  - 单计时器（通常对应 base 分组），超时回退重传整窗
  - 接收方只接受按序分组，乱序直接丢弃

**可靠性机制（可讲清楚的工程点）**
- Checksum：检测 corruption（损坏）
- Sliding Window：控制 in-flight 分组数量
- Timeout & Retransmission：应对丢包

**可量化指标（报告中已有/可补齐）**
- goodput（有效吞吐），以及丢包/损坏率变化下 SR vs GBN 的差异
- 重传次数、超时次数、平均/峰值窗口占用

**在整合项目中的作用**
- 作为“可靠性/性能分析”的硬核部分：提供可复现实验与数据
- 为 Lab2 的“性能监控”提供指标定义与展示内容（重传、吞吐等）

### 1.2 Lab2（`lab2/`）

Lab2 的定位是“面向业务功能的网络服务”，并且天然贴近后台开发：协议设计、文件操作、错误处理、以及跨网络环境差异。

**协议抽象（按 handout）**
- 两类消息结构：
  - `CmdMsgT`：command / filename / size / port / error（注意网络字节序 `hton*` / `ntoh*`）
  - `DataMsgT`：`data[DATABUFLEN]`（DATABUFLEN=3000）

**命令与语义**
- `ls`：客户端请求 server backup 目录的文件列表；server 返回文件数（command msg）+ 文件名（data msg）
- `send <filename>`：将本地文件上传到 server backup 目录；需要处理小/大文件、覆盖确认、传输完成确认
- `remove <filename>`：删除 server 端文件；需要区分“不存在/权限/成功”等错误码
- `rename <old> <new>`：重命名 server 端文件；需要处理新名字冲突、原文件不存在
- `quit`：客户端退出
- `shutdown`：关闭 server

**交互与日志（按 handout）**
- Server：每次进入 Waiting 状态打印 `Waiting UDP command @: xxxxx`
- Server：收到命令打印 `[CMD RECEIVED]: xxx`
- Client：不支持命令打印 ` - wrong command`

**已知工程风险（整合项目要重点修复/强化）**
- 跨网络时序/竞态：尤其是 `rename`（需要显式握手或消息合并，避免“客户端发送过快/乱序”）
- 可观测性不足：目前偏“能跑”，需要补齐 metrics 与日志让它“可运维”
- Docker 端口策略：若 server 采用动态 TCP 端口，需要为容器化做明确方案（端口范围映射或改为固定端口）

---

## 2. 目标形态（最终交付物）

### 2.1 “对用户可用”的交付
- CLI 客户端（保留现有交互，作为底座）
- 服务端（保留现有 UDP/TCP 机制）
- **Web 管理台（新增）**：
  - 文件管理：列出文件、上传、删除、重命名
  - 性能监控：展示吞吐、请求延迟、错误率、传输统计
- **Docker 部署（新增）**：
  - `docker compose up` 一键启动 server + web

### 2.2 “对简历/面试可讲”的技术点
- 协议设计：命令/数据分离、大小文件策略（UDP vs TCP）
- 稳定性设计：校验、超时、重试、握手、幂等
- 观测与运维：指标 + 日志 + health check
- 工程化：容器化、配置化、可复现测试

### 2.3 三阶段实现计划（A/B/C 技术选型落地）

这里把技术选型拆成“三阶段交付”，每一阶段都能独立形成可演示成果；你可以按阶段逐步往上叠加工程能力，同时确保每一步都能完全消化。

#### 阶段一（A：工程化底座，最接近大厂研发流程）

**目标**：先把“核心业务 + 工程化”做扎实：能编译、能跑、能定位问题、能自动化验证。

**技术栈（A）**
- C++17
- POSIX sockets（UDP/TCP）
- CMake 构建
- 日志：`spdlog`
- 测试：GoogleTest（或 Catch2 二选一）
- Docker（至少能 `docker build`）
- GitHub Actions：自动 build + test + docker build 验证

**实现内容（A）**
- 迁入/改写 Lab2 的 `server` / `client` 为本项目的 C++ 代码基线
- 明确并实现错误码与日志字段（建议至少包含：cmd、filename、error_code、latency_ms）
- 修复/强化 `rename` 的时序问题（显式握手或消息合并），保证跨网络/有延迟时稳定
- 写最小单元测试：
  - 文件名校验（防目录穿越）
  - 关键协议字段序列化/反序列化（字节序）

**验收标准（A）**
- CLI 全命令可用：`ls/send/remove/rename/shutdown/quit`
- 关键路径有日志；遇到错误能明确返回 error code
- CI 能在干净环境完成：build + unit tests + docker build

#### 阶段二（B：可观测 + Web 管理台，贴近“线上服务形态”）

**目标**：把项目从“能跑的服务”升级成“可运维的服务”：有管理入口、有 metrics、有健康检查，便于演示与排障。

**技术栈（B）**
- HTTP：C++ 内嵌轻量 HTTP Server（推荐 `cpp-httplib`）
- JSON：`nlohmann/json`
- Web 前端：静态 HTML + 少量 JS（不引入大型前端框架，控制复杂度）

**实现内容（B）**
- Server 增加 HTTP 管理/观测接口：
  - `GET /healthz`
  - `GET /api/files`
  - `POST /api/upload`
  - `DELETE /api/files/{name}`
  - `POST /api/rename`
  - `GET /api/metrics`（JSON）
- Metrics 至少覆盖：
  - `requests_total{cmd}`、`errors_total{cmd,code}`
  - `bytes_received_total`、`send_sessions_total`
  - `latency_ms`（先做 avg/p95 或简单 bucket 均可）
- 端口策略为 Docker 友好：
  - 推荐将文件传输 TCP 端口改为“固定端口 + 并发处理”，避免映射一段动态端口范围
- 增加集成测试：启动 server → 通过 HTTP API 跑一组黑盒用例，并校验 metrics 增量
- Docker Compose 一键启动：`docker compose up` 后即可打开 Web 页面演示

**验收标准（B）**
- 浏览器可完成文件列表/上传/删除/重命名
- Web 能看到 metrics（请求数、错误数、吞吐/字节数、延迟）
- 集成测试在 CI 上可跑通

#### 阶段三（C：可选“更大厂”的增强项，按精力挑选）

**目标**：在不破坏可控复杂度的前提下，挑 1~3 个增强项做“加分”，用于面试深挖。

**可选增强（C）**
- 并发模型升级：线程池/任务队列；限制并发与资源占用（更像真实后端）
- Metrics 升级：增加 Prometheus `/metrics` 文本输出（或导出更细粒度直方图）
- 扰动/压测脚本：
  - `tc netem` 注入 delay/loss
  - 产出 SR vs GBN 的对照数据（把 Lab1 的优势“产品化”展示）
- 发布增强：
  - GitHub Actions Release（tag 触发）上传二进制 artifacts
  - 可选推送镜像到 GHCR（对外展示更完整）

**验收标准（C）**
- 任意一项增强有：清晰文档 + 可复现实验/演示步骤 + 可量化结果（例如吞吐提升、失败率下降、p95 延迟变化）

---

## 3. 整体架构设计（新增组件最小化）

### 3.1 组件划分
- `backup-server`（现有 `server.cc` 增量改造）
  - UDP：接收命令
  - TCP：文件数据（大文件）
  - 本地 `backup/` 存储
  - 输出指标（metrics）

- `backup-client`（现有 `client.cc` 增量改造）
  - 保持 CLI（用于验收与回归）

- `web-console`（新增，建议“C++ 内嵌 HTTP + 静态前端”）
  - Server 直接提供 HTTP API 与静态页面（HTML/JS）
  - 避免引入重前端框架与额外语言栈，降低理解成本，同时仍贴近大厂服务的“管理台 + 观测接口”形态

> 建议选项：**(2) server 提供 HTTP API**，减少 Web 端重复实现 UDP/TCP 协议细节，降低 bug 面。

### 3.2 数据流
- 用户（浏览器）→ Web → HTTP → Server
- Server 执行文件操作/传输 → 更新 metrics → Web 拉取展示

---

## 4. Web 管理台（简单但“像后台”）

### 4.1 页面（最小集合）
- `/`：文件列表 + 操作入口
- `/upload`：上传文件（可直接在首页提供上传控件）
- `/metrics`：性能指标页面（表格/简单折线图可后续迭代；先文本/表格即可）

### 4.2 HTTP API 设计（server 侧或 web 侧）
- `GET /api/files` → 返回文件列表
- `POST /api/upload` → 上传文件
- `DELETE /api/files/{name}` → 删除
- `POST /api/rename`（body: old,new）→ 重命名
- `GET /api/metrics` → 获取指标快照
- `GET /healthz` → 健康检查

### 4.3 安全与边界（简历可提但不做过度）
- Demo 默认只暴露在本机/容器网络
- 文件名校验：禁止 `../`、绝对路径
- 限制最大上传大小（配置项）

---

## 5. 性能监控（Metrics 方案）

### 5.1 指标定义（建议至少实现这些）
**请求维度**
- `requests_total{cmd}`：各命令次数（ls/send/remove/rename）
- `errors_total{cmd,code}`：错误次数
- `latency_ms_bucket{cmd}`：延迟直方图（或简单 avg/p95）

**传输维度（send）**
- `bytes_received_total`
- `send_sessions_total`
- `send_failures_total`
- `throughput_bytes_per_sec`（会话级或滑动窗口）

**可靠性维度（若实现/复用 Lab1）**
- `retransmissions_total`
- `packet_loss_estimate`（若能统计）

### 5.2 实现方式（两种可选）
- A) 直接在 server 内部维护 counters + 暴露 `GET /api/metrics` JSON
- B) Prometheus 风格 `/metrics` 文本（加分，但不是必须）

> 建议先实现 A，后续可升级成 Prometheus。

---

## 6. 稳定性强化（让项目更“后台”）

### 6.1 修复/增强 rename 的时序问题
- 目标：跨机器、存在延迟/乱序时依然可靠
- 策略：
  - 显式握手：Server 收到 `CMD_RENAME(old)` 后返回 `READY_FOR_NEW_NAME`，Client/Web 再发送 new name
  - 或将 old/new 一次性放在同一条消息（若协议允许扩展字段）

### 6.2 幂等与一致性（最小改造）
- `remove`：删除不存在文件应返回可区分错误码
- `send`：同名覆盖必须二次确认（已存在机制，Web 需映射）

---

## 7. Docker 化部署（可复现演示）

### 7.1 镜像/服务规划
- `server` 容器：运行 backup server（UDP + TCP + HTTP API）
- `web` 容器：运行 Flask Web Console

### 7.2 端口与网络
- UDP：server 命令端口（例如 35887/udp）
- TCP：server 数据端口范围（例如 40000-40100/tcp，或改成单端口+会话复用）
- HTTP：server API（例如 8080/tcp，仅容器内或对外暴露）
- Web：对外暴露 8000/tcp

> 注意：Docker 下“动态分配大量 TCP 端口”不太友好。
> 方案：
> - 方案1：限制 TCP 端口范围并在 compose 显式映射
> - 方案2（推荐）：将 TCP 传输改为单固定端口 + Server 多线程/多进程接入（更像生产系统）

### 7.3 数据卷
- 将 `backup/` 目录挂载为 volume，容器重启数据不丢

---

## 8. 代码落点（按你的要求：直接在本文件夹内完成）

后续所有新增/改写代码、Docker 与文档，都直接放在本目录：
- `Computer_Network_lab/reliable-remote-backup-system/`

同时保留原始 `lab1/` 与 `lab2/` 不动，作为作业原始版本与对照。

---

## 9. 测试与发布流程（让项目“可验证、可交付”）

这一节是把项目“作业化”提升为“后台工程项目”的关键：不仅能跑，还要能自动化验证、可回归、可发布。

### 9.1 测试分层（建议至少覆盖到集成测试）

**A. 静态检查（最快反馈）**
- C/C++：编译参数启用 `-Wall -Wextra`（可进一步启用 `-Werror` 作为 CI 阻断）
- Python（若 Web 用 Flask）：`ruff` 或 `flake8`（二选一）
- 可选：`clang-format` / `black` 作为格式化约束

**B. 单元测试（Unit Tests）**
- 优先覆盖“纯逻辑”模块，避免依赖网络环境：
  - 文件名校验（禁止 `../`、绝对路径、非法字符）
  - metrics 聚合逻辑（计数器、吞吐计算、滑动窗口统计）
  - 协议字段序列化/反序列化（size/port 字节序、结构体长度）

**C. 集成测试（Integration Tests，强烈建议）**
- 启动 server（本机或 Docker）后，以黑盒方式跑一组用例（CLI 或 Web API 任一）：
  - `ls`：空/非空
  - `send`：小文件/大文件；覆盖确认分支
  - `remove`：存在/不存在
  - `rename`：成功/冲突/不存在；包含“延迟环境”下的稳定性
- 核心断言：
  - 文件内容一致（hash/size）
  - 返回错误码符合预期
  - metrics 有对应增量（requests_total、errors_total、bytes_received_total、latency）

**D. 扰动测试（可选加分）**
- Linux 上用 `tc netem` 注入 delay/loss（或在 server 增加人工延迟开关）
- 产出一份可复现报告：延迟/丢包率 vs 吞吐/失败率/重传次数

### 9.2 GitHub Actions（CI）设计

建议在仓库根目录新增 `.github/workflows/ci.yml`，触发条件为 `push` 与 `pull_request`。

**CI 应至少包含以下 Job（思路版，不绑定具体构建系统）**
- `build`：
  - Ubuntu runner
  - 编译 C/C++（server/client）
  - 若有 Web（Python），安装依赖并做基本检查
- `lint`（可并入 build 或单独 job）：
  - C/C++：可选 `clang-tidy`（不做也可）
  - Python：`ruff`/`flake8`
- `test-unit`：运行单元测试
- `test-integration`：
  - 启动 server（后台进程或 `docker compose up -d`）
  - 执行 `scripts/integration_test.*`
  - 拉取 `/api/metrics` 校验关键指标变化
- `docker-build`：
  - 构建 Docker 镜像（server/web）
  - 仅验证可构建，不要求推送

### 9.3 发布（Release）与交付物

**发布触发建议**
- 基于 git tag（如 `v1.0.0`）触发 `.github/workflows/release.yml`

**Release workflow 目标**
- 构建并上传 artifacts：
  - CLI 可执行文件（或压缩包）
  - Docker Compose 文件与示例配置
- 可选：推送镜像到 GHCR（GitHub Container Registry）
  - `ghcr.io/<user>/<repo>-server:<tag>`
  - `ghcr.io/<user>/<repo>-web:<tag>`

**交付物的“面试友好”标准**
- `docker compose up` 可以直接跑起来
- `README` 给出：
  - 端口说明
  - 演示路径（Web 操作与 CLI 操作各一条）
  - metrics 示例输出

### 9.4 验收清单（你自己/面试官都能快速验证）
- `ls`：空目录提示、非空列出
- `send`：小文件/大文件均可上传；覆盖确认
- `remove`：存在/不存在
- `rename`：存在/冲突/不存在
- `shutdown`：server 正常退出

### 9.5 稳定性与性能验收
- 延迟注入：模拟跨机器延迟（可用 Linux `tc netem`，或在代码中加入 artificial delay 进行 demo）
- 丢包注入：验证 SR/GBN 指标（如果把 Lab1 作为可选“对照实验”）
- Web metrics：能看到请求次数、错误次数、传输吞吐

---

## 10. 文档产出（面试/简历加分）

建议最终补齐以下文档（放到 `docs/`）：
- `README.md`：一键启动 + 演示截图
- `architecture.md`：架构图、端口、数据流
- `api.md`：HTTP API 定义
- `observability.md`：指标定义与含义
- `debugging-story.md`：rename 竞态定位与修复过程（强烈建议）

---

## 11. 下一步（需要你确认的 3 个工程决策）

1) Web 与 server 的交互方式：
- 选 A：Web 复用 UDP/TCP 协议（更贴合原作业，但实现更复杂）
- 选 B：server 新增 HTTP API（更工程化，最推荐）

2) TCP 端口策略（Docker 相关）：
- 选 A：固定端口范围 + compose 显式映射
- 选 B：改为单固定端口 + 并发处理（更像生产）

3) Metrics 输出格式：
- 选 A：JSON（快速落地）
- 选 B：Prometheus 文本（更“后台”加分）

确认后我可以按本计划开始实际改造与落地实现。
