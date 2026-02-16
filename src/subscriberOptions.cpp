#include "uDataPacketService/subscriberOptions.hpp"

using namespace UDataPacketService;

class SubscriberOptions::SubscriberOptionsImpl
{
public:

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

/// Destructor
SubscriberOptions::~SubscriberOptions() = default;
