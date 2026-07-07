#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include "uDataPacketService/metricsSingleton.hpp"
 
using namespace UDataPacketService::Metrics;

MetricsSingleton &MetricsSingleton::getInstance()
{
    std::mutex mutex;
    const std::scoped_lock lock{mutex};
    static MetricsSingleton instance;
    return instance;
}   

void MetricsSingleton::incrementReceivedPacketsCounter() noexcept
{   
    mReceivedPacketsCounter.fetch_add(1, std::memory_order_relaxed);
}   

int64_t MetricsSingleton::getReceivedPacketsCount() const noexcept
{   
    return mReceivedPacketsCounter.load(std::memory_order_relaxed);
}   

void MetricsSingleton::incrementSentPacketsCounter() noexcept
{
    mSentPacketsCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getSentPacketsCount() const noexcept
{
    return mSentPacketsCounter.load(std::memory_order_relaxed);
}

void MetricsSingleton::updateUtilization(double utilization)
{
    mUtilization.store(std::min(std::max(0.0, utilization), 1.0));
}

double MetricsSingleton::getUtilization() const noexcept
{
    return mUtilization.load();
}                      

void MetricsSingleton::resetCounters()
{   
    mReceivedPacketsCounter.store(0);
    mSentPacketsCounter.store(0);
    mUtilization.store(0);
}   

void UDataPacketService::Metrics::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}


