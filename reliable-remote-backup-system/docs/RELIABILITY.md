# Reliability Semantics & Design Decisions

This document describes the reliability guarantees, consistency model, and operational boundaries of the Reliable Remote Backup System. It is intended for technical reviewers and interviewers who want to understand the design trade-offs.

---

## 1. Delivery Semantics

### Command Channel (UDP)

| Command | Semantics | Idempotency | Notes |
|---------|-----------|-------------|-------|
| `ls` | At-most-once | ✅ Idempotent | Read-only, safe to retry |
| `send` | At-least-once | ⚠️ Conditional | Overwrites existing file; repeated sends produce same result |
| `remove` | At-least-once | ✅ Idempotent | Deleting non-existent file returns success |
| `rename` | At-most-once | ❌ Not idempotent | Uses explicit handshake to prevent race conditions |
| `shutdown` | At-most-once | ✅ Idempotent | Server ignores if already shutting down |

### Why UDP for Commands?

- Commands are small (<256 bytes) and latency-sensitive
- Application-level acknowledgment provides reliability where needed
- Simpler than maintaining persistent TCP connections for sporadic commands

### File Transfer Channel (TCP)

- **Guaranteed delivery**: TCP handles retransmission, ordering, and flow control
- **Explicit handshake**: Server sends port assignment, client connects, both confirm before transfer
- **Atomic write**: Files are written completely or not at all (no partial files on error)

---

## 2. Consistency Model

### File Operations

| Operation | Consistency Guarantee | Implementation |
|-----------|----------------------|----------------|
| Upload | Last-write-wins | Direct overwrite; no versioning |
| Delete | Immediate | File removed synchronously before response |
| Rename | Atomic | Uses `std::filesystem::rename` (POSIX atomic on same filesystem) |
| List | Read-committed | Returns files visible at query time |

### Concurrent Access

**Current limitations (single-server, single-threaded command processing):**

- Commands are processed sequentially in the main UDP loop
- HTTP requests are handled in a separate thread with shared filesystem access
- No distributed locking or coordination

**Race condition mitigations:**

1. **Rename handshake**: Client and server perform explicit ACK exchange before rename completes, preventing the original lab's race condition where rename could execute before file upload finished.

2. **File existence checks**: Delete and rename verify file exists before operating.

3. **Filename validation**: Happens before any filesystem operation.

```
Original Lab Bug (Fixed):
  Client: send file.txt    ─────────────────────────────▶ (upload completes)
  Client: rename file.txt newname.txt ──▶ (rename starts)
                                          ❌ Race: rename might execute
                                             before upload ACK received

Fixed Design:
  Client: send file.txt    ─────────────────────────────▶ (upload completes)
  Client: rename file.txt newname.txt ──▶
  Server: ◀── ACK (ready to rename)
  Client: ──▶ CONFIRM
  Server: (rename executes) ──▶ SUCCESS
```

---

## 3. Failure Handling

### Network Failures

| Scenario | Behavior |
|----------|----------|
| Client timeout waiting for response | Client may retry; server state depends on whether operation completed |
| TCP connection drops during upload | Partial file may remain; client should retry entire upload |
| Server crash during operation | No durability guarantee; in-flight operations lost |

### Error Propagation

- All operations return explicit error codes (see `error_codes.h`)
- HTTP API returns structured JSON with `success` boolean and `error` message
- Metrics track errors by type for operational visibility

---

## 4. Resource Boundaries

### Concurrency Limits

| Resource | Limit | Rationale |
|----------|-------|-----------|
| Concurrent UDP commands | 1 (sequential) | Simplicity; adequate for single-client scenarios |
| Concurrent TCP transfers | 10 (port range 40000-40010) | Docker port mapping constraint |
| HTTP concurrent requests | ~100 (cpp-httplib default) | Thread pool managed by library |

### Size Limits

| Resource | Default Limit | Configurable |
|----------|---------------|--------------|
| Max upload size (HTTP) | 100 MB | `HttpServerConfig::maxUploadSize` |
| Max filename length | 128 chars | Protocol constraint (`CmdMsg::filename`) |
| Backup directory | No limit | Filesystem constraint |

### Timeout Values

| Operation | Timeout | Location |
|-----------|---------|----------|
| UDP command response | 5 seconds | Client-side |
| TCP connection setup | 10 seconds | Client-side |
| HTTP request | 30 seconds | cpp-httplib default |

---

## 5. Security Boundaries

### Input Validation

All filenames are validated before any operation:

```cpp
bool isValidFilename(const std::string& filename) {
    // Reject empty names
    // Reject path separators (/, \)
    // Reject parent directory references (..)
    // Reject Windows reserved names (CON, PRN, NUL, etc.)
    // Reject illegal characters (<>:"|?*)
    // Reject names starting/ending with space or dot
}
```

### Trust Boundaries

| Interface | Trust Level | Validation |
|-----------|-------------|------------|
| UDP commands | Untrusted | Filename validation, size checks |
| HTTP API | Untrusted | Same as UDP + content-type validation |
| Filesystem | Trusted | Operations confined to backup directory |

---

## 6. Observability for Troubleshooting

### Metrics for Problem Detection

| Symptom | Relevant Metrics | Investigation |
|---------|------------------|---------------|
| Slow uploads | `latency.http_upload.p95_ms` | Check if consistently high or spiky |
| Failed transfers | `transfer.send_failures` | Cross-reference with error logs |
| High error rate | `errors.*` by type | Identify most common failure mode |

### Log Correlation

Currently, logs include:
- Timestamp
- Log level
- Component (server/client)
- Message with context (filename, size, error)

**Planned improvement**: Add request ID to correlate multi-step operations:
```
[2026-01-06 10:30:00] [INFO] [req-abc123] Upload started: file.txt
[2026-01-06 10:30:01] [INFO] [req-abc123] Upload completed: 1048576 bytes
```

---

## 7. Known Limitations & Future Work

| Limitation | Impact | Planned Solution |
|------------|--------|------------------|
| No request ID tracing | Hard to correlate logs for a single request | Add UUID per request |
| Single-server only | No horizontal scaling | Out of scope for this project |
| No authentication | Anyone on network can access | Add token-based auth for production |
| Last-write-wins | No conflict detection | Add ETag/version headers |
| No persistent queue | In-flight operations lost on crash | Add WAL for critical operations |

---

## 8. Interview Discussion Points

If asked about this system in an interview, I can discuss:

1. **Why this design?** Trade-offs between simplicity and reliability for a learning project
2. **How would you scale it?** Stateless HTTP layer + shared storage (S3/NFS) + load balancer
3. **How would you add exactly-once semantics?** Client-generated request IDs + server-side deduplication
4. **How would you handle partial failures?** Saga pattern or compensating transactions
5. **What's missing for production?** Auth, TLS, rate limiting, persistent queuing, distributed tracing
