#include <string>
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/streamOptions.hpp"

using namespace UDataPacketService;

class SubscriptionManagerOptions::SubscriptionManagerOptionsImpl
{
public:
    StreamOptions mStreamOptions;
    int mMaximumNumberOfSubscribers{16};
};

/// Constructor
SubscriptionManagerOptions::SubscriptionManagerOptions() : 
    pImpl(std::make_unique<SubscriptionManagerOptionsImpl> ()) 
{
}

/// Copy constructor
SubscriptionManagerOptions::SubscriptionManagerOptions(
    const SubscriptionManagerOptions &options)
{
    *this = options;
}

/// Move constructor
SubscriptionManagerOptions::SubscriptionManagerOptions(
    SubscriptionManagerOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
SubscriptionManagerOptions& SubscriptionManagerOptions::operator=(
    const SubscriptionManagerOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<SubscriptionManagerOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
SubscriptionManagerOptions& SubscriptionManagerOptions::operator=(
    SubscriptionManagerOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
SubscriptionManagerOptions::~SubscriptionManagerOptions() = default;

/// Stream options
void SubscriptionManagerOptions::setStreamOptions(const StreamOptions &options)
{
    pImpl->mStreamOptions = options;
}

StreamOptions SubscriptionManagerOptions::getStreamOptions() const noexcept
{
    return pImpl->mStreamOptions;
}

/// Max subscribers
void SubscriptionManagerOptions::setMaximumNumberOfSubscribers(
    const int maxSubscribers)
{
    if (maxSubscribers <= 0)
    {
        throw std::invalid_argument(
            "Max number of subscribers must be positive");
    } 
    pImpl->mMaximumNumberOfSubscribers = maxSubscribers;
}

int SubscriptionManagerOptions::getMaximumNumberOfSubscribers() const noexcept 
{
    return pImpl->mMaximumNumberOfSubscribers;
}


