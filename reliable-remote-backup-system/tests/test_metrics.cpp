#include <gtest/gtest.h>
#include "common/metrics.h"
#include <thread>
#include <chrono>

using namespace backup;

class MetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        Metrics::instance().reset();
    }
};

TEST_F(MetricsTest, RecordRequestIncrementsCounter) {
    Metrics::instance().recordRequest("ls");
    Metrics::instance().recordRequest("ls");
    Metrics::instance().recordRequest("send");
    
    EXPECT_EQ(Metrics::instance().getRequestCount("ls"), 2);
    EXPECT_EQ(Metrics::instance().getRequestCount("send"), 1);
    EXPECT_EQ(Metrics::instance().getRequestCount("remove"), 0);
}

TEST_F(MetricsTest, RecordErrorIncrementsCounter) {
    Metrics::instance().recordError("send", "FILE_NOT_FOUND");
    Metrics::instance().recordError("send", "TIMEOUT");
    Metrics::instance().recordError("remove", "FILE_NOT_FOUND");
    
    EXPECT_EQ(Metrics::instance().getErrorCount("send"), 2);
    EXPECT_EQ(Metrics::instance().getErrorCount("remove"), 1);
}

TEST_F(MetricsTest, RecordBytesReceivedAccumulates) {
    Metrics::instance().recordBytesReceived(100);
    Metrics::instance().recordBytesReceived(200);
    Metrics::instance().recordBytesReceived(300);
    
    EXPECT_EQ(Metrics::instance().getBytesReceived(), 600);
}

TEST_F(MetricsTest, RecordBytesSentAccumulates) {
    Metrics::instance().recordBytesSent(1000);
    Metrics::instance().recordBytesSent(2000);
    
    EXPECT_EQ(Metrics::instance().getBytesSent(), 3000);
}

TEST_F(MetricsTest, RecordSendSessionAndFailures) {
    Metrics::instance().recordSendSession();
    Metrics::instance().recordSendSession();
    Metrics::instance().recordSendFailure();
    
    EXPECT_EQ(Metrics::instance().getSendSessions(), 2);
    EXPECT_EQ(Metrics::instance().getSendFailures(), 1);
}

TEST_F(MetricsTest, RecordLatencyCalculatesAverage) {
    Metrics::instance().recordLatency("test", 10.0);
    Metrics::instance().recordLatency("test", 20.0);
    Metrics::instance().recordLatency("test", 30.0);
    
    double avg = Metrics::instance().getAverageLatency("test");
    EXPECT_NEAR(avg, 20.0, 0.001);
}

TEST_F(MetricsTest, RecordLatencyTracksMax) {
    Metrics::instance().recordLatency("test", 10.0);
    Metrics::instance().recordLatency("test", 50.0);
    Metrics::instance().recordLatency("test", 30.0);
    
    EXPECT_EQ(Metrics::instance().getMaxLatency("test"), 50.0);
}

TEST_F(MetricsTest, ScopedTimerRecordsLatency) {
    {
        auto timer = Metrics::instance().startTimer("scoped_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    double avg = Metrics::instance().getAverageLatency("scoped_test");
    EXPECT_GE(avg, 10.0);  // Should be at least 10ms
}

TEST_F(MetricsTest, HttpRequestIncrementsCounter) {
    Metrics::instance().recordHttpRequest("/api/files", "GET");
    Metrics::instance().recordHttpRequest("/api/upload", "POST");
    Metrics::instance().recordHttpRequest("/api/files", "DELETE");
    
    EXPECT_EQ(Metrics::instance().getHttpRequestCount(), 3);
}

TEST_F(MetricsTest, UptimeIncreases) {
    uint64_t uptime1 = Metrics::instance().getUptimeSeconds();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t uptime2 = Metrics::instance().getUptimeSeconds();
    
    EXPECT_GE(uptime2 - uptime1, 1);
}

TEST_F(MetricsTest, ResetClearsAllMetrics) {
    Metrics::instance().recordRequest("ls");
    Metrics::instance().recordBytesReceived(1000);
    Metrics::instance().recordSendSession();
    
    Metrics::instance().reset();
    
    EXPECT_EQ(Metrics::instance().getRequestCount("ls"), 0);
    EXPECT_EQ(Metrics::instance().getBytesReceived(), 0);
    EXPECT_EQ(Metrics::instance().getSendSessions(), 0);
}

TEST_F(MetricsTest, ConcurrentAccessIsSafe) {
    const int numThreads = 10;
    const int numOperations = 1000;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([numOperations]() {
            for (int j = 0; j < numOperations; j++) {
                Metrics::instance().recordRequest("concurrent");
                Metrics::instance().recordBytesReceived(1);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(Metrics::instance().getRequestCount("concurrent"), 
              numThreads * numOperations);
    EXPECT_EQ(Metrics::instance().getBytesReceived(), 
              numThreads * numOperations);
}
