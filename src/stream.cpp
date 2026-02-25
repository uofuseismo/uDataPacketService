#include <mutex>
#include <memory>
#include <algorithm>
#include <queue>
#include <cmath>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <oneapi/tbb/concurrent_map.h>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"

import Utilities;

using namespace UDataPacketService;

class Stream::StreamImpl
{
public:
    /// Constructor
    StreamImpl(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options,
               std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger),
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
        std::lock_guard<std::mutex> lock(mMutex);
        mMostRecentPacket = std::move(packet);
        mHaveMostRecentPacket = true;
        for (auto &it : mSubscribersMap)
        {
            if (it.second.size() >= mMaximumQueueSize)
            {
                it.second.pop(); 
            }
            auto packetCopy = mMostRecentPacket;
            it.second.push(packetCopy);
        }
    }

    /// Subscriber gets next packet
    [[nodiscard]] std::optional<UDataPacketServiceAPI::V1::Packet>
        getNextPacket(const uintptr_t contextAddress) noexcept
    {   
        std::optional<UDataPacketServiceAPI::V1::Packet> result{std::nullopt};
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
        return result;
    }   

    /// Subscribe to the stream 
    [[nodiscard]] bool subscribe(const uintptr_t contextAddress,
                                 const bool enqueueLatestPacket)
    {
        auto contextAddressString = std::to_string(contextAddress);
        bool wasAdded{false};
        if (!mSubscribersMap.contains(contextAddress))
        {
            std::pair
            <
                uintptr_t,
                std::queue<UDataPacketServiceAPI::V1::Packet>
            > newElement;
            newElement.first = contextAddress;
            if (enqueueLatestPacket && mHaveMostRecentPacket)
            {
                newElement.second.push(mMostRecentPacket);
            }
            auto [it, added] = mSubscribersMap.insert(std::move(newElement));
            if (!added)
            {
                wasAdded = false;
#ifndef NDEBUG
                if (mLogger)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Couldn't add new subscriber {} to {}",
                                       contextAddressString,
                                       mStreamIdentifier);
                }
#endif
            }
            else
            {
                wasAdded = true;
            }
        }
        // All done
        if (wasAdded)
        {
#ifndef NDEBUG
            if (mLogger)
            {
                SPDLOG_LOGGER_DEBUG(mLogger, "{} subscribed to {}", 
                                    contextAddressString, mStreamIdentifier);
            }
#endif
        }
        return wasAdded;
    }

    /// Unsubscribes from the stream.
    /// @result True indicates the context was subscribed and is not
    ///         unsubscribed.
    [[nodiscard]] bool unsubscribe(const uintptr_t contextAddress)
    {
        // Erase is not thread safe
        size_t originalSize{0};
        size_t newSize{0};
        bool wasUnsubscribed = false;
        {
        std::lock_guard<std::mutex> lock(mMutex);
        originalSize = mSubscribersMap.size();
        size_t exists = mSubscribersMap.unsafe_erase(contextAddress);
        newSize = mSubscribersMap.size();
        if (exists == 1){wasUnsubscribed = true;}
        }
        if (wasUnsubscribed)
        {
            if (newSize + 1 != originalSize)
            {
                throw std::runtime_error(
                    "Unexpected behavior during unsubscribe");
            }
#ifndef NDEBUG
            if (mLogger)
            {
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "{} unsubscribed from {}",
                                     contextAddressString, mStreamIdentifier);
            }
#endif
        }
        else
        {
            if (newSize != originalSize)
            {   
                throw std::runtime_error(
                   "Unexpected resize during unsubscribe");
            }   
#ifndef NDEBUG
            if (mLogger)
            {
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "{} never subscribed to {}",
                                     contextAddressString, mStreamIdentifier);
            }
#endif
        }
        return wasUnsubscribed;
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
        return mSubscribersMap.size();
    }   

    /// The current subscribers.
    std::set<uintptr_t> getSubscribers() const noexcept
    {   
        std::vector<uintptr_t> keys;
        keys.reserve(mSubscribersMap.size());
        for (const auto &item : mSubscribersMap)
        {   
            keys.push_back(item.first);
        }   
        // Convert to a set
        std::set<uintptr_t> result;
        for (const auto &key : keys)
        {
            result.insert(key);
        }   
        return result;
    }
    /// @result True indicates this subscriber is subscribed.
    [[nodiscard]] bool isSubscribed(const uintptr_t contextAddress) const noexcept
    {   
        return mSubscribersMap.contains(contextAddress);
    }   

//private:
    mutable std::mutex mMutex;
    StreamOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    tbb::concurrent_map<uintptr_t, std::queue<UDataPacketServiceAPI::V1::Packet>> mSubscribersMap;
    UDataPacketServiceAPI::V1::Packet mMostRecentPacket;
    std::string mStreamIdentifier;
    size_t mMaximumQueueSize{8};
    bool mHaveMostRecentPacket{false};
};

Stream::Stream(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options) :
    pImpl(std::make_unique<StreamImpl> (std::move(packet), options))
{
}

Stream::Stream(UDataPacketServiceAPI::V1::Packet &&packet,
               const StreamOptions &options,
               std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<StreamImpl> (std::move(packet), options, logger))
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

bool Stream::unsubscribe(const uintptr_t contextAddress)
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

/// Destructor
Stream::~Stream() = default;
