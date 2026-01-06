# Computer Network Lab — Reliable Remote Backup System

[![CI](https://github.com/FrankieLiu04/Computer_Network_lab/actions/workflows/ci.yml/badge.svg)](https://github.com/FrankieLiu04/Computer_Network_lab/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED?logo=docker&logoColor=white)](https://www.docker.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> A production-style remote file backup system built from scratch using C++17, demonstrating network programming, systems design, and cloud-native practices.

---

## 🎯 Project Motivation

This project originated from a university network programming assignment. Rather than submitting a minimal solution, I treated it as an opportunity to **learn and practice real-world backend engineering skills**:

| Original Assignment | What I Built |
|---------------------|--------------|
| Basic UDP file transfer | Dual-channel architecture: UDP for commands + TCP for reliable data transfer |
| Console-only interaction | RESTful HTTP API + Web management dashboard |
| Manual testing | Automated CI/CD with GitHub Actions + unit/integration tests |
| Local execution only | Docker containerization with one-command deployment |
| No observability | Structured logging + real-time metrics (request counts, latency percentiles, throughput) |

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Client                                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────────────┐  │
│  │   CLI       │    │  Commands   │    │  File Data (TCP)                │  │
│  │  Interface  │───▶│   (UDP)     │───▶│  - Explicit handshake           │  │
│  └─────────────┘    └─────────────┘    │  - Timeout & retry              │  │
│                                        │  - Progress tracking            │  │
│                                        └─────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Server                                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────────────┐  │
│  │  UDP Cmd    │    │   Command   │    │  HTTP Server (cpp-httplib)      │  │
│  │  Listener   │───▶│   Handler   │◀──▶│  - REST API (/api/files, etc.)  │  │
│  │  :35887     │    │             │    │  - Web UI (static/index.html)   │  │
│  └─────────────┘    └─────────────┘    │  - Health check (/healthz)      │  │
│                            │           │  - Metrics (/api/metrics)       │  │
│                            ▼           └─────────────────────────────────┘  │
│                     ┌─────────────┐                                         │
│                     │  Metrics    │◀── Request counts, latency (p50/p95),   │
│                     │  Collector  │    bytes transferred, error rates       │
│                     └─────────────┘                                         │
│                            │                                                │
│                            ▼                                                │
│                     ┌─────────────┐                                         │
│                     │  Backup     │    Validated filenames, atomic writes   │
│                     │  Storage    │                                         │
│                     └─────────────┘                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **UDP for commands, TCP for data** | Commands are small and latency-sensitive; file transfers need reliability and flow control |
| **Explicit handshake before transfer** | Prevents race conditions in rename/remove operations (a bug I fixed from the original lab) |
| **Singleton Metrics collector** | Thread-safe, lock-free counters for high-frequency operations; mutex-protected for percentile sampling |
| **Filename validation layer** | Defense against path traversal (`../`), reserved names, and injection attacks |
| **Separation of HTTP and UDP interfaces** | HTTP for human/dashboard access; UDP for programmatic client access |

---

## 🛠️ Technical Highlights

### 1. Network Programming (C++ Socket API)

- **Dual-protocol design**: UDP command channel + dynamic TCP data channel
- **Non-trivial protocol**: Custom binary message format (`CmdMsg`, `DataMsg`) with explicit field packing
- **Error handling**: Timeout, retry, and graceful degradation

```cpp
// Example: Explicit handshake to prevent race conditions
// Server waits for client ACK before proceeding with rename
sendto(sock, &ackMsg, sizeof(ackMsg), 0, clientAddr, addrLen);
recvfrom(sock, &response, sizeof(response), 0, ...);  // Wait for client confirmation
```

### 2. Observability & Metrics

- **Structured logging** with spdlog (file + console, configurable levels)
- **Real-time metrics collection**:
  - Request counts by command type
  - Error counts by error code
  - Latency tracking with percentile calculation (p50, p95, max)
  - Throughput (bytes received/sent)

```cpp
// RAII-based latency measurement
auto timer = Metrics::instance().startTimer("http_upload");
// ... operation executes ...
// Timer automatically records latency on scope exit
```

### 3. Cloud-Native Practices

- **Docker multi-stage build**: Minimized image size, reproducible builds
- **Docker Compose**: One-command deployment with health checks
- **GitHub Actions CI**: Automated build + test on Linux and Windows

```yaml
# Health check ensures container is truly ready
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:8080/healthz"]
  interval: 10s
  timeout: 5s
  retries: 3
```

### 4. Security Considerations

| Threat | Mitigation |
|--------|------------|
| Path traversal (`../../../etc/passwd`) | Strict filename validation, reject any path separators |
| Reserved filename injection (Windows) | Block `CON`, `PRN`, `NUL`, etc. |
| Oversized uploads | Configurable `maxUploadSize` limit |
| Malformed requests | Input validation at protocol and HTTP layers |

---

## 🚀 Quick Start

### Option 1: Docker (Recommended)

```bash
cd reliable-remote-backup-system
docker compose up -d

# Open Web UI
# http://localhost:8080

# Test the API
curl http://localhost:8080/healthz
curl http://localhost:8080/api/files
```

### Option 2: Build from Source

```bash
cd reliable-remote-backup-system
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel

# Run tests
ctest --output-on-failure

# Start server
./backup-server -port 35887 -http 8080
```

---

## 📊 API Reference

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/healthz` | GET | Health check (for container orchestration) |
| `/api/files` | GET | List all backed-up files |
| `/api/upload` | POST | Upload file (multipart/form-data) |
| `/api/files/{name}` | DELETE | Delete a file |
| `/api/rename` | POST | Rename a file (`{"oldName": "a", "newName": "b"}`) |
| `/api/metrics` | GET | Real-time performance metrics |

<details>
<summary>Example: Metrics Response</summary>

```json
{
  "uptime_seconds": 3600,
  "requests": {
    "ls": 42,
    "send": 15,
    "http_upload": 8
  },
  "errors": {
    "invalid_filename": 2,
    "file_not_found": 1
  },
  "transfer": {
    "bytes_received": 10485760,
    "send_sessions": 15,
    "send_failures": 0
  },
  "latency": {
    "http_upload": {
      "avg_ms": 125.3,
      "max_ms": 450.0,
      "p95_ms": 280.0
    }
  }
}
```

</details>

---

## 🧪 Testing Strategy

| Layer | Tool | Coverage |
|-------|------|----------|
| Unit tests | GoogleTest | Filename validation, error codes, metrics logic |
| Integration tests | Bash + curl | HTTP API end-to-end flows |
| CI | GitHub Actions | Linux (Ubuntu 22.04) + Windows builds |

```bash
# Run integration tests
docker compose up -d
bash scripts/integration_test.sh
```

---

## 📈 Development Roadmap

### ✅ Phase 1: Engineering Foundation
- [x] C++17 + CMake build system
- [x] Structured logging (spdlog)
- [x] Unit testing (GoogleTest)
- [x] GitHub Actions CI/CD
- [x] Security: filename validation

### ✅ Phase 2: Web Dashboard + Observability
- [x] HTTP REST API
- [x] Web file management UI
- [x] Metrics collection (requests, errors, latency, throughput)
- [x] Docker Compose deployment
- [x] Integration test suite

### 🔜 Phase 3: Production Hardening (Planned)
- [ ] Prometheus metrics export (`/metrics`)
- [ ] Request ID tracing for log correlation
- [ ] Connection pooling / thread pool for concurrent transfers
- [ ] Chaos testing with `tc netem` (network delay/loss simulation)

---

## 📁 Project Structure

```
reliable-remote-backup-system/
├── include/                    # Header files
│   ├── client/                 # Client interfaces
│   ├── common/                 # Shared utilities (metrics, logging, file ops)
│   ├── protocol/               # Wire protocol definitions
│   └── server/                 # Server components (HTTP, command handler)
├── src/                        # Implementation
├── static/                     # Web UI (index.html)
├── scripts/                    # Build and test scripts
├── tests/                      # Unit tests
├── .github/workflows/          # CI configuration
├── Dockerfile                  # Server container
├── docker-compose.yml          # Orchestration config
└── docs/                       # Additional documentation
    ├── DOCKER_QUICKSTART.md
    └── LOCAL_RUN_GUIDE.md
```

---

## 🔧 What I Learned

Through this project, I gained hands-on experience with:

1. **Low-level network programming**: Socket APIs, protocol design, handling partial reads/writes
2. **Systems thinking**: Trade-offs between UDP/TCP, designing for failure modes
3. **Observability**: Why metrics matter, how to instrument code without overhead
4. **DevOps practices**: Docker, CI/CD, infrastructure-as-code mindset
5. **Security awareness**: Input validation, defense in depth

---

## 📄 License

MIT License — feel free to use this as a reference or starting point.

---

**Author**: [Frankie Liu](https://github.com/FrankieLiu04)  
**Repository**: [Computer_Network_lab](https://github.com/FrankieLiu04/Computer_Network_lab)
