#include "uDataPacketService/streamOptions.hpp"

using namespace UDataPacketService;

#define DEFAULT_QUEUE_SIZE 8

class StreamOptions::StreamOptionsImpl
{
public:
    int mMaximumQueueSize{DEFAULT_QUEUE_SIZE};
};

/// Constructor
StreamOptions::StreamOptions() :
    pImpl(std::make_unique<StreamOptionsImpl> ())
{
}

/// Copy constructor
StreamOptions::StreamOptions(const StreamOptions &options)
{
    *this = options;
}

/// Move constructor
StreamOptions::StreamOptions(StreamOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
StreamOptions& StreamOptions::operator=(const StreamOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<StreamOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
StreamOptions& StreamOptions::operator=(StreamOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
StreamOptions::~StreamOptions() = default;

/// Queue size
void StreamOptions::setMaximumQueueSize(const int queueSize)
{   
    if (queueSize <= 0)
    {
        throw std::invalid_argument("Queue size must be positive");
    }
    pImpl->mMaximumQueueSize = queueSize;
}

int StreamOptions::getMaximumQueueSize() const noexcept
{
    return pImpl->mMaximumQueueSize;
}   

