#ifndef UDATA_PACKET_SERVICE_METRICS_SINGLETON_HPP
#define UDATA_PACKET_SERVICE_METRICS_SINGLETON_HPP
#include <atomic>
#include <cstdint>

namespace UDataPacketService::Metrics
{

class MetricsSingleton
{
public:
    static MetricsSingleton &getInstance();
    void incrementReceivedPacketsCounter() noexcept;

    [[nodiscard]] int64_t getReceivedPacketsCount() const noexcept;
    void incrementSentPacketsCounter() noexcept;

    [[nodiscard]] int64_t getSentPacketsCount() const noexcept;

    void updateUtilization(double utilization);
    [[nodiscard]] double getUtilization() const noexcept;

    void resetCounters();
private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
    std::atomic<double> mUtilization{0};
    std::atomic<int64_t> mReceivedPacketsCounter{0};
    std::atomic<int64_t> mSentPacketsCounter{0};
};

void initializeMetricsSingleton();

}


#endif
