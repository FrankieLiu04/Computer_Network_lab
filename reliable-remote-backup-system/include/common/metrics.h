#ifndef BACKUP_COMMON_METRICS_H
#define BACKUP_COMMON_METRICS_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace backup {

/**
 * Thread-safe metrics collection for the backup system.
 * Tracks requests, errors, bytes transferred, and latency.
 */
class Metrics {
public:
    // Singleton instance
    static Metrics& instance();

    // Disable copy
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    // ==========================================================================
    // Request metrics
    // ==========================================================================
    
    /**
     * Record a command request
     * @param cmd Command name (ls, send, remove, rename, shutdown)
     */
    void recordRequest(const std::string& cmd);

    /**
     * Record a command error
     * @param cmd Command name
     * @param errorCode Error code as string
     */
    void recordError(const std::string& cmd, const std::string& errorCode);

    /**
     * Get total request count for a command
     */
    uint64_t getRequestCount(const std::string& cmd) const;

    /**
     * Get total error count for a command
     */
    uint64_t getErrorCount(const std::string& cmd) const;

    /**
     * Get all request counts
     */
    std::unordered_map<std::string, uint64_t> getAllRequestCounts() const;

    /**
     * Get all error counts
     */
    std::unordered_map<std::string, uint64_t> getAllErrorCounts() const;

    // ==========================================================================
    // Transfer metrics
    // ==========================================================================

    /**
     * Record bytes received
     * @param bytes Number of bytes received
     */
    void recordBytesReceived(uint64_t bytes);

    /**
     * Record bytes sent
     * @param bytes Number of bytes sent
     */
    void recordBytesSent(uint64_t bytes);

    /**
     * Record a completed send session
     */
    void recordSendSession();

    /**
     * Record a failed send session
     */
    void recordSendFailure();

    /**
     * Get total bytes received
     */
    uint64_t getBytesReceived() const { return bytesReceived_.load(); }

    /**
     * Get total bytes sent
     */
    uint64_t getBytesSent() const { return bytesSent_.load(); }

    /**
     * Get total send sessions
     */
    uint64_t getSendSessions() const { return sendSessions_.load(); }

    /**
     * Get total send failures
     */
    uint64_t getSendFailures() const { return sendFailures_.load(); }

    // ==========================================================================
    // Latency metrics
    // ==========================================================================

    /**
     * RAII timer for measuring latency
     */
    class ScopedTimer {
    public:
        ScopedTimer(Metrics& metrics, const std::string& cmd);
        ~ScopedTimer();

        // Disable copy
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

        // Move constructor
        ScopedTimer(ScopedTimer&& other) noexcept;

    private:
        Metrics& metrics_;
        std::string cmd_;
        std::chrono::steady_clock::time_point start_;
        bool moved_ = false;
    };

    /**
     * Create a scoped timer for measuring command latency
     */
    ScopedTimer startTimer(const std::string& cmd);

    /**
     * Record a latency measurement
     * @param cmd Command name
     * @param latencyMs Latency in milliseconds
     */
    void recordLatency(const std::string& cmd, double latencyMs);

    /**
     * Get average latency for a command
     */
    double getAverageLatency(const std::string& cmd) const;

    /**
     * Get max latency for a command
     */
    double getMaxLatency(const std::string& cmd) const;

    /**
     * Get latency percentile (approximate)
     * @param cmd Command name
     * @param percentile Percentile (0-100)
     */
    double getLatencyPercentile(const std::string& cmd, double percentile) const;

    // ==========================================================================
    // HTTP metrics
    // ==========================================================================

    /**
     * Record an HTTP request
     * @param endpoint API endpoint
     * @param method HTTP method
     */
    void recordHttpRequest(const std::string& endpoint, const std::string& method);

    /**
     * Get total HTTP request count
     */
    uint64_t getHttpRequestCount() const { return httpRequests_.load(); }

    // ==========================================================================
    // System metrics
    // ==========================================================================

    /**
     * Get server uptime in seconds
     */
    uint64_t getUptimeSeconds() const;

    /**
     * Get start time as Unix timestamp
     */
    uint64_t getStartTime() const;

    /**
     * Reset all metrics (for testing)
     */
    void reset();

private:
    Metrics();
    ~Metrics() = default;

    // Request counters
    mutable std::mutex requestMutex_;
    std::unordered_map<std::string, std::atomic<uint64_t>*> requestCounts_;
    std::unordered_map<std::string, std::atomic<uint64_t>*> errorCounts_;

    // Transfer counters
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> sendSessions_{0};
    std::atomic<uint64_t> sendFailures_{0};

    // Latency tracking (simple histogram approach)
    struct LatencyStats {
        std::atomic<uint64_t> count{0};
        std::atomic<double> sum{0.0};
        std::atomic<double> max{0.0};
        std::vector<double> samples;  // For percentile calculation
        mutable std::mutex sampleMutex;
        static constexpr size_t MAX_SAMPLES = 1000;
    };
    mutable std::mutex latencyMutex_;
    std::unordered_map<std::string, LatencyStats*> latencyStats_;

    // HTTP request counter
    std::atomic<uint64_t> httpRequests_{0};

    // Start time
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::system_clock::time_point startTimeSystem_;

    // Helper to get or create request counter
    std::atomic<uint64_t>& getOrCreateCounter(
        std::unordered_map<std::string, std::atomic<uint64_t>*>& map,
        const std::string& key);

    // Helper to get or create latency stats
    LatencyStats& getOrCreateLatencyStats(const std::string& cmd);
};

}  // namespace backup

#endif  // BACKUP_COMMON_METRICS_H
