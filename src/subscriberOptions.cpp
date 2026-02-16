#include <vector>
#include <algorithm>
#include <chrono>
#include "uDataPacketService/subscriberOptions.hpp"
#include "uDataPacketService/grpcOptions.hpp"

using namespace UDataPacketService;

class SubscriberOptions::SubscriberOptionsImpl
{
public:
    GRPCOptions mGRPCOptions;
    std::vector<std::chrono::milliseconds> mReconnectSchedule
    {
        std::chrono::seconds {0},
        std::chrono::seconds {5},
        std::chrono::seconds {15}
    };
};

/// Constructor
SubscriberOptions::SubscriberOptions() :
    pImpl(std::make_unique<SubscriberOptionsImpl> ())
{
}

/// Copy constructor
SubscriberOptions::SubscriberOptions(const SubscriberOptions &options)
{
    *this = options;
}

/// Move constructor
SubscriberOptions::SubscriberOptions(SubscriberOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
SubscriberOptions& 
SubscriberOptions::operator=(const SubscriberOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<SubscriberOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
SubscriberOptions& 
SubscriberOptions::operator=(SubscriberOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// GRPC options
void SubscriberOptions::setGRPCOptions(const GRPCOptions &options)
{
    pImpl->mGRPCOptions = options;
}

GRPCOptions SubscriberOptions::getGRPCOptions() const
{
    return pImpl->mGRPCOptions;
}

/// Reconnect schedule
void SubscriberOptions::setReconnectSchedule(
    const std::vector<std::chrono::milliseconds> &schedule)
{
    if (std::any_of(schedule.begin(), schedule.end(), [](const auto &x)
                    {
                        return x.count() < 0;
                    }))
    {
        throw std::invalid_argument(
           "At least one value in reconnect scheudle is negative");
    }
    pImpl->mReconnectSchedule = schedule;
    std::sort(pImpl->mReconnectSchedule.begin(),
              pImpl->mReconnectSchedule.end()); 
}

std::vector<std::chrono::milliseconds> 
SubscriberOptions::getReconnectSchedule() const noexcept
{
    return pImpl->mReconnectSchedule;
}

/// Destructor
SubscriberOptions::~SubscriberOptions() = default;
