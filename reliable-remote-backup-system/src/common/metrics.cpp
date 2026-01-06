#include "common/metrics.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace backup {

Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

Metrics::Metrics() 
    : startTime_(std::chrono::steady_clock::now()),
      startTimeSystem_(std::chrono::system_clock::now()) {
}

// =============================================================================
// Request metrics
// =============================================================================

void Metrics::recordRequest(const std::string& cmd) {
    getOrCreateCounter(requestCounts_, cmd).fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordError(const std::string& cmd, const std::string& errorCode) {
    std::string key = cmd + ":" + errorCode;
    getOrCreateCounter(errorCounts_, key).fetch_add(1, std::memory_order_relaxed);
    getOrCreateCounter(errorCounts_, cmd).fetch_add(1, std::memory_order_relaxed);
}

uint64_t Metrics::getRequestCount(const std::string& cmd) const {
    std::lock_guard<std::mutex> lock(requestMutex_);
    auto it = requestCounts_.find(cmd);
    if (it != requestCounts_.end()) {
        return it->second->load(std::memory_order_relaxed);
    }
    return 0;
}

uint64_t Metrics::getErrorCount(const std::string& cmd) const {
    std::lock_guard<std::mutex> lock(requestMutex_);
    auto it = errorCounts_.find(cmd);
    if (it != errorCounts_.end()) {
        return it->second->load(std::memory_order_relaxed);
    }
    return 0;
}

std::unordered_map<std::string, uint64_t> Metrics::getAllRequestCounts() const {
    std::lock_guard<std::mutex> lock(requestMutex_);
    std::unordered_map<std::string, uint64_t> result;
    for (const auto& pair : requestCounts_) {
        result[pair.first] = pair.second->load(std::memory_order_relaxed);
    }
    return result;
}

std::unordered_map<std::string, uint64_t> Metrics::getAllErrorCounts() const {
    std::lock_guard<std::mutex> lock(requestMutex_);
    std::unordered_map<std::string, uint64_t> result;
    for (const auto& pair : errorCounts_) {
        result[pair.first] = pair.second->load(std::memory_order_relaxed);
    }
    return result;
}

// =============================================================================
// Transfer metrics
// =============================================================================

void Metrics::recordBytesReceived(uint64_t bytes) {
    bytesReceived_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::recordBytesSent(uint64_t bytes) {
    bytesSent_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::recordSendSession() {
    sendSessions_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordSendFailure() {
    sendFailures_.fetch_add(1, std::memory_order_relaxed);
}

// =============================================================================
// Latency metrics
// =============================================================================

Metrics::ScopedTimer::ScopedTimer(Metrics& metrics, const std::string& cmd)
    : metrics_(metrics), cmd_(cmd), start_(std::chrono::steady_clock::now()) {
}

Metrics::ScopedTimer::~ScopedTimer() {
    if (!moved_) {
        auto end = std::chrono::steady_clock::now();
        double latencyMs = std::chrono::duration<double, std::milli>(end - start_).count();
        metrics_.recordLatency(cmd_, latencyMs);
    }
}

Metrics::ScopedTimer::ScopedTimer(ScopedTimer&& other) noexcept
    : metrics_(other.metrics_), cmd_(std::move(other.cmd_)), start_(other.start_) {
    other.moved_ = true;
}

Metrics::ScopedTimer Metrics::startTimer(const std::string& cmd) {
    return ScopedTimer(*this, cmd);
}

void Metrics::recordLatency(const std::string& cmd, double latencyMs) {
    auto& stats = getOrCreateLatencyStats(cmd);
    
    // Update count and sum atomically
    stats.count.fetch_add(1, std::memory_order_relaxed);
    
    // Update sum (not perfectly thread-safe but acceptable for metrics)
    double oldSum = stats.sum.load(std::memory_order_relaxed);
    while (!stats.sum.compare_exchange_weak(oldSum, oldSum + latencyMs,
                                            std::memory_order_relaxed)) {}
    
    // Update max
    double oldMax = stats.max.load(std::memory_order_relaxed);
    while (latencyMs > oldMax && 
           !stats.max.compare_exchange_weak(oldMax, latencyMs,
                                            std::memory_order_relaxed)) {}
    
    // Store sample for percentile calculation
    {
        std::lock_guard<std::mutex> lock(stats.sampleMutex);
        if (stats.samples.size() < LatencyStats::MAX_SAMPLES) {
            stats.samples.push_back(latencyMs);
        } else {
            // Reservoir sampling
            static thread_local std::mt19937 gen(std::random_device{}());
            size_t count = stats.count.load(std::memory_order_relaxed);
            std::uniform_int_distribution<size_t> dis(0, count - 1);
            size_t idx = dis(gen);
            if (idx < LatencyStats::MAX_SAMPLES) {
                stats.samples[idx] = latencyMs;
            }
        }
    }
}

double Metrics::getAverageLatency(const std::string& cmd) const {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    auto it = latencyStats_.find(cmd);
    if (it != latencyStats_.end()) {
        uint64_t count = it->second->count.load(std::memory_order_relaxed);
        if (count > 0) {
            return it->second->sum.load(std::memory_order_relaxed) / count;
        }
    }
    return 0.0;
}

double Metrics::getMaxLatency(const std::string& cmd) const {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    auto it = latencyStats_.find(cmd);
    if (it != latencyStats_.end()) {
        return it->second->max.load(std::memory_order_relaxed);
    }
    return 0.0;
}

double Metrics::getLatencyPercentile(const std::string& cmd, double percentile) const {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    auto it = latencyStats_.find(cmd);
    if (it == latencyStats_.end()) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> sampleLock(it->second->sampleMutex);
    if (it->second->samples.empty()) {
        return 0.0;
    }
    
    std::vector<double> sorted = it->second->samples;
    std::sort(sorted.begin(), sorted.end());
    
    size_t idx = static_cast<size_t>(std::ceil(percentile / 100.0 * sorted.size())) - 1;
    idx = std::min(idx, sorted.size() - 1);
    return sorted[idx];
}

// =============================================================================
// HTTP metrics
// =============================================================================

void Metrics::recordHttpRequest(const std::string& endpoint, const std::string& method) {
    httpRequests_.fetch_add(1, std::memory_order_relaxed);
    std::string key = method + ":" + endpoint;
    getOrCreateCounter(requestCounts_, key).fetch_add(1, std::memory_order_relaxed);
}

// =============================================================================
// System metrics
// =============================================================================

uint64_t Metrics::getUptimeSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();
}

uint64_t Metrics::getStartTime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        startTimeSystem_.time_since_epoch()).count();
}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(requestMutex_);
    
    for (auto& pair : requestCounts_) {
        pair.second->store(0, std::memory_order_relaxed);
    }
    for (auto& pair : errorCounts_) {
        pair.second->store(0, std::memory_order_relaxed);
    }
    
    bytesReceived_.store(0, std::memory_order_relaxed);
    bytesSent_.store(0, std::memory_order_relaxed);
    sendSessions_.store(0, std::memory_order_relaxed);
    sendFailures_.store(0, std::memory_order_relaxed);
    httpRequests_.store(0, std::memory_order_relaxed);
    
    {
        std::lock_guard<std::mutex> latencyLock(latencyMutex_);
        for (auto& pair : latencyStats_) {
            pair.second->count.store(0, std::memory_order_relaxed);
            pair.second->sum.store(0.0, std::memory_order_relaxed);
            pair.second->max.store(0.0, std::memory_order_relaxed);
            std::lock_guard<std::mutex> sampleLock(pair.second->sampleMutex);
            pair.second->samples.clear();
        }
    }
    
    startTime_ = std::chrono::steady_clock::now();
    startTimeSystem_ = std::chrono::system_clock::now();
}

// =============================================================================
// Helper functions
// =============================================================================

std::atomic<uint64_t>& Metrics::getOrCreateCounter(
    std::unordered_map<std::string, std::atomic<uint64_t>*>& map,
    const std::string& key) {
    
    std::lock_guard<std::mutex> lock(requestMutex_);
    auto it = map.find(key);
    if (it == map.end()) {
        auto* counter = new std::atomic<uint64_t>(0);
        map[key] = counter;
        return *counter;
    }
    return *it->second;
}

Metrics::LatencyStats& Metrics::getOrCreateLatencyStats(const std::string& cmd) {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    auto it = latencyStats_.find(cmd);
    if (it == latencyStats_.end()) {
        auto* stats = new LatencyStats();
        latencyStats_[cmd] = stats;
        return *stats;
    }
    return *it->second;
}

}  // namespace backup
