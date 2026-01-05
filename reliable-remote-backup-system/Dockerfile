# =============================================================================
# Multi-stage Dockerfile for Reliable Remote Backup System
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1: Build
# -----------------------------------------------------------------------------
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel $(nproc)

# -----------------------------------------------------------------------------
# Stage 2: Runtime
# -----------------------------------------------------------------------------
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user (check if exists first, as some base images have 'backup' user)
RUN id -u backupapp > /dev/null 2>&1 || useradd -m -s /bin/bash backupapp

# Create directories
RUN mkdir -p /app/backup /app/logs && \
    chown -R backupapp:backupapp /app

WORKDIR /app

# Copy binaries from builder
COPY --from=builder /app/build/backup-server /app/
COPY --from=builder /app/build/backup-client /app/

# Set ownership
RUN chown backup:backup /app/backup-server /app/backup-client && \
    chmod +x /app/backup-server /app/backup-client

# Switch to non-root user
USER backupapp

# Default command
CMD ["./backup-server", "-port", "35887", "-backup", "/app/backup", "-log"]

# Expose ports
# UDP command port
EXPOSE 35887/udp
# TCP file transfer port range
EXPOSE 40000-40100/tcp
