module;
#include <iostream>
#include <mutex>
#include <memory>
#include <algorithm>
#include <queue>
#include <cmath>
#include <spdlog/spdlog.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/concurrent_map.h>
#include <oneapi/tbb/parallel_for.h>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"

import Utilities;
export module Stream;

namespace UDataPacketService
{

constexpr size_t MINIMUM_QUEUE_SIZE = 1;
constexpr size_t DEFAULT_QUEUE_SIZE = 8;

export 
class StreamOptions
{
public:
    void setMaximumQueueSize(const int queueSize)
    {
        if (queueSize <= 0)
        {
            throw std::invalid_argument("Queue size must be positive");
        }
        mMaximumQueueSize = queueSize;
    }
    [[nodiscard]] int getMaximumQueueSize() const noexcept
    {
       return mMaximumQueueSize;
    }
private:
    int mMaximumQueueSize{DEFAULT_QUEUE_SIZE};
};

/// @brief A stream is used to export data packets for a given 
///        NETWORK.STATION.CHANNEL.LOCATION_CODE.  The publisher will
///        create the stream and add packets while the subscriber will
///        attempt to get the next packet.
export
class Stream
{
public:
    Stream(UDataPacketServiceAPI::V1::Packet &&packet,
           const StreamOptions &options,
           std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger),
        mMaximumQueueSize(mOptions.getMaximumQueueSize())
    {
        mStreamIdentifier = Utilities::toName(packet);
        setNextPacket(std::move(packet));
    }
    explicit Stream(UDataPacketServiceAPI::V1::Packet &&packet,
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
    void setNextPacket(const UDataPacketServiceAPI::V1::Packet &packet)
    {
        auto copy = packet;
        setNextPacket(std::move(copy));
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
    /// @brief Unsubscribes from the stream.
    /// @param[in] contextAddress  The identifier to unsubscribe.
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
    /// @brief Subscriber gets next packet
    /// @param[in] contextAddress  The subscriber's identifier.
    /// @result The next packet if it exists.  Otherwise, std::nullopt.
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
    /// @result The number of subscribers.
    [[nodiscard]] int getNumberOfSubscribers() const noexcept
    {
        return mSubscribersMap.size();
    }
    /// @result The current subscribers.
    [[nodiscard]] std::set<uintptr_t> getSubscribers() const noexcept
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
    [[nodiscard]] bool operator<(const std::string &identifier) const
    {
        return identifier < mStreamIdentifier;
    }
    [[nodiscard]] bool operator==(const std::string &identifier) const
    {
        return identifier == mStreamIdentifier;
    }
    Stream() = delete;
    ~Stream() = default;
//private:
    mutable std::mutex mMutex;
    StreamOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    tbb::concurrent_map<uintptr_t, std::queue<UDataPacketServiceAPI::V1::Packet>> mSubscribersMap;
    //PacketQueueHashMap mSubscriberHashMap;
    UDataPacketServiceAPI::V1::Packet mMostRecentPacket;
    std::string mStreamIdentifier;
    size_t mMaximumQueueSize{DEFAULT_QUEUE_SIZE};
    bool mHaveMostRecentPacket{false};
};

class SubscriptionManager
{
public:
    void subscribe()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        
    }
    mutable std::mutex mMutex;
    std::set<Stream> mStreams; 
};
 
}
