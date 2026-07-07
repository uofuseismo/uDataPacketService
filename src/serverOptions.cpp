#include <algorithm>
#include <chrono>
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"

using namespace UDataPacketService;

class ServerOptions::ServerOptionsImpl
{
public:
    GRPCServerOptions mGRPCOptions;
    SubscriptionManagerOptions mSubscriptionManagerOptions;
    std::chrono::milliseconds mShutdownDeadline{1000};
    std::chrono::milliseconds mMaximumWriterPollInterval{250};
    int mMaximumNumberOfSubscribers{8};
};

/// Constructor
ServerOptions::ServerOptions() :
    pImpl(std::make_unique<ServerOptionsImpl> ())
{
}

/// Copy constructor
ServerOptions::ServerOptions(const ServerOptions &options)
{
    *this = options;
}

/// Move constructor
ServerOptions::ServerOptions(ServerOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
ServerOptions& 
ServerOptions::operator=(const ServerOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<ServerOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
ServerOptions& 
ServerOptions::operator=(ServerOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// GRPC options
void ServerOptions::setGRPCOptions(const GRPCServerOptions &options)
{
    pImpl->mGRPCOptions = options;
}

GRPCServerOptions ServerOptions::getGRPCOptions() const
{
    return pImpl->mGRPCOptions;
}

/// Max subscribers
void ServerOptions::setMaximumNumberOfSubscribers(const int maxSubscribers)
{
    if (maxSubscribers <= 0)
    {
        throw std::invalid_argument(
            "Max number of subscribers must be positive");
    } 
    pImpl->mMaximumNumberOfSubscribers = maxSubscribers;
}

int ServerOptions::getMaximumNumberOfSubscribers() const noexcept
{
    return pImpl->mMaximumNumberOfSubscribers;
}

/// Shutdown deadline
void ServerOptions::setShutdownDeadline(
    const std::chrono::milliseconds &deadline)
{
    if (deadline.count() <= 0)
    {
        throw std::invalid_argument("Shutdown deadline must be positive");
    }
    pImpl->mShutdownDeadline = deadline;
    // Side effect: an idle writer must always wake and finish before the
    // server starts forcibly cancelling RPCs at the shutdown deadline
    pImpl->mMaximumWriterPollInterval
        = std::min(pImpl->mMaximumWriterPollInterval, deadline);
}

std::chrono::milliseconds ServerOptions::getShutdownDeadline() const noexcept
{
    return pImpl->mShutdownDeadline;
}

/// Maximum writer poll interval
void ServerOptions::setMaximumWriterPollInterval(
    const std::chrono::milliseconds &interval)
{
    if (interval.count() <= 0)
    {
        throw std::invalid_argument("Poll interval must be positive");
    }
    if (interval >= pImpl->mShutdownDeadline)
    {
        throw std::invalid_argument(
            "Poll interval must be less than the shutdown deadline ("
          + std::to_string(pImpl->mShutdownDeadline.count()) + " ms)");
    }
    pImpl->mMaximumWriterPollInterval = interval;
}

std::chrono::milliseconds
ServerOptions::getMaximumWriterPollInterval() const noexcept
{
    return pImpl->mMaximumWriterPollInterval;
}

/// Subscription manager options
void ServerOptions::setSubscriptionManagerOptions(
    const SubscriptionManagerOptions &options)
{
    pImpl->mSubscriptionManagerOptions = options;
}

SubscriptionManagerOptions ServerOptions::getSubscriptionManagerOptions() const
{
    return pImpl->mSubscriptionManagerOptions;
}


/// Destructor
ServerOptions::~ServerOptions() = default;
