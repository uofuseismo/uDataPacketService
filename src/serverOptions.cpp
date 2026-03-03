#include <algorithm>
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"

using namespace UDataPacketService;

class ServerOptions::ServerOptionsImpl
{
public:
    GRPCServerOptions mGRPCOptions;
    SubscriptionManagerOptions mSubscriptionManagerOptions;
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
