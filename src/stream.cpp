#include <cstddef>
#include <cstdint>
#include <cmath>
#include <map>
#include <mutex>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketService/utilities.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

using namespace UDataPacketService;

class Stream::StreamImpl
{
public:
    /// Constructor
    StreamImpl(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options,
               std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger)),
        mMaximumQueueSize(mOptions.getMaximumQueueSize())
    {   
        mStreamIdentifier = Utilities::toName(packet);
        setNextPacket(std::move(packet));
    }   

    /// Constructor
    StreamImpl(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options) :
        mOptions(options),
        mLogger(nullptr),
        mMaximumQueueSize(mOptions.getMaximumQueueSize())
    {   
        mStreamIdentifier = Utilities::toName(packet);
        setNextPacket(std::move(packet));
    }   

    /// Sets the next packet
    void setNextPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        auto thisIdentifier = Utilities::toName(packet);
        if (thisIdentifier != mStreamIdentifier)
        {
            throw std::runtime_error(thisIdentifier
                                   + " does not match stream identifier "
                                   + mStreamIdentifier);
        }
        // Set the next packets
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        mMostRecentPacket = std::move(packet);
        mHaveMostRecentPacket = true;
        for (auto &it : mSubscribersMap)
        {
            if (it.second.size() >= mMaximumQueueSize)
            {
                it.second.pop(); 
            }
            UDataPacketServiceAPI::V1::Packet packetCopy{mMostRecentPacket};
            it.second.push(std::move(packetCopy));
        }
        }
    }

    /// Subscriber gets next packet
    [[nodiscard]] std::optional<UDataPacketServiceAPI::V1::Packet>
        getNextPacket(const uintptr_t contextAddress) noexcept
    {
        std::optional<UDataPacketServiceAPI::V1::Packet> result{std::nullopt};
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mSubscribersMap.find(contextAddress);
        if (idx != mSubscribersMap.end())
        {
            if (!idx->second.empty())
            {
                result
                    = std::make_optional<UDataPacketServiceAPI::V1::Packet>
                      (std::move(idx->second.front()));
                idx->second.pop();
            }
        }
        }
        return result;
    }   

    /// Subscribe to the stream 
    [[nodiscard]] bool subscribe(const uintptr_t contextAddress,
                                 const bool enqueueLatestPacket)
    {
        bool wasAdded{false};
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto [it, added] = mSubscribersMap.try_emplace(contextAddress);
        if (added && enqueueLatestPacket && mHaveMostRecentPacket)
        {
            it->second.push(mMostRecentPacket);
        }
        wasAdded = added;
        }
        if (wasAdded)
        {
            if (mLogger)
            {
                SPDLOG_LOGGER_DEBUG(mLogger, "{} subscribed to {}",
                                    std::to_string(contextAddress),
                                    mStreamIdentifier);
            }
        }
        return wasAdded;
    }

    /// Unsubscribes from the stream.
    /// @result True indicates the context was subscribed.
    ///         False indicates the context failed to be unsubscribed.
    [[nodiscard]]
    Stream::UnsubscribeResponse unsubscribe(const uintptr_t contextAddress)
    {
        size_t numberErased{0};
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        numberErased = mSubscribersMap.erase(contextAddress);
        }
        auto result = (numberErased == 1) ?
                      Stream::UnsubscribeResponse::Unsubscribed :
                      Stream::UnsubscribeResponse::NeverSubscribed;
        if (mLogger)
        {
            if (result == Stream::UnsubscribeResponse::Unsubscribed)
            {
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "{} unsubscribed from {}",
                                    std::to_string(contextAddress),
                                    mStreamIdentifier);
            }
            else
            {
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "{} never subscribed to {}",
                                    std::to_string(contextAddress),
                                    mStreamIdentifier);
            }
        }
        return result;
    }

    /// Forcefully purge all subscribers
    void unsubscribeAll()
    {
        {
        const std::lock_guard<std::mutex> lock(mMutex);        
        mSubscribersMap.clear();
        }
    }

    /// Sets the next packet
    void setNextPacket(const UDataPacketServiceAPI::V1::Packet &packet)
    {   
        auto copy = packet;
        setNextPacket(std::move(copy));
    }   

    /// The number of subscribers.
    int getNumberOfSubscribers() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        return static_cast<int> (mSubscribersMap.size());
    }

    /// The current subscribers.
    std::set<uintptr_t> getSubscribers() const noexcept
    {
        std::set<uintptr_t> result;
        const std::lock_guard<std::mutex> lock(mMutex);
        for (const auto &item : mSubscribersMap)
        {
            result.insert(item.first);
        }
        return result;
    }
    /// @result True indicates this subscriber is subscribed.
    [[nodiscard]] bool isSubscribed(const uintptr_t contextAddress) const noexcept
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        return mSubscribersMap.contains(contextAddress);
    }

//private:
    // mMutex guards the subscribers map, its queues, and the
    // most-recent-packet state; hold it in every function touching these.
    mutable std::mutex mMutex;
    std::map
    <
        uintptr_t,
        std::queue<UDataPacketServiceAPI::V1::Packet>
    > mSubscribersMap;
    UDataPacketServiceAPI::V1::Packet mMostRecentPacket;
    bool mHaveMostRecentPacket{false};
    // Immutable after construction
    StreamOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::string mStreamIdentifier;
    size_t mMaximumQueueSize{8};
};

Stream::Stream(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options) :
    pImpl(std::make_unique<StreamImpl> (std::move(packet), options))
{
}

Stream::Stream(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options,
               std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<StreamImpl> (std::move(packet),
                                        options,
                                        std::move(logger)))
{
}

void Stream::setNextPacket(UDataPacketServiceAPI::V1::Packet &&packet)
{
    pImpl->setNextPacket(std::move(packet));
}

void Stream::setNextPacket(const UDataPacketServiceAPI::V1::Packet &packet)
{
    pImpl->setNextPacket(packet);
}

std::optional<UDataPacketServiceAPI::V1::Packet>
    Stream::getNextPacket(const uintptr_t contextAddress) noexcept
{
    return pImpl->getNextPacket(contextAddress);
}

bool Stream::subscribe(const uintptr_t contextAddress,
                       const bool enqueueLatestPacket)
{
    return pImpl->subscribe(contextAddress, enqueueLatestPacket);
}

Stream::UnsubscribeResponse Stream::unsubscribe(const uintptr_t contextAddress)
{
    return pImpl->unsubscribe(contextAddress);
}

int Stream::getNumberOfSubscribers() const noexcept
{
    return pImpl->getNumberOfSubscribers();
}

std::set<uintptr_t> Stream::getSubscribers() const noexcept
{
    return pImpl->getSubscribers();
}
 
bool Stream::isSubscribed(const uintptr_t contextAddress) const noexcept
{
    return pImpl->isSubscribed(contextAddress);
}

std::string Stream::getIdentifier() const noexcept
{
    return pImpl->mStreamIdentifier;
}

void Stream::unsubscribeAll()
{
    pImpl->unsubscribeAll();
}

/// Destructor
Stream::~Stream() = default;
