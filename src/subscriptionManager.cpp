#include <chrono>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketService/utilities.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/data_type.pb.h"

using namespace UDataPacketService;

class SubscriptionManager::SubscriptionManagerImpl
{
public:
    SubscriptionManagerImpl(const SubscriptionManagerOptions &options,
                            std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger)),
        mStreamOptions(mOptions.getStreamOptions())
    {
    }
 
    /// Add packet (and, if it is a new stream, update subscribers)
    void enqueuePacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        auto streamIdentifier = Utilities::toName(packet);
        // Hot path: the stream exists.  Look up its address under the lock
        // then deliver outside it - streams are never removed from the map
        // so the pointer stays valid and packet delivery only contends on
        // the stream's own lock.
        Stream *existingStream{nullptr};
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mStreamsMap.find(streamIdentifier);
        if (idx != mStreamsMap.end()){existingStream = idx->second.get();}
        }
        if (existingStream)
        {
            try
            {
                existingStream->setNextPacket(std::move(packet));
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(
                    "Subscription manager failed to enqueue "
                  + streamIdentifier + " because " + std::string {e.what()});
            }
            return;
        }
        // Do it the hard way
        std::unique_ptr<Stream> stream{nullptr};
        try
        {
            stream
                = std::make_unique<Stream> (std::move(packet), mStreamOptions);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to create stream because "
                                   + std::string {e.what()});
        }
#ifndef NDEBUG
        assert(stream != nullptr);
#endif
        SPDLOG_LOGGER_DEBUG(mLogger, "Adding {}", streamIdentifier);
        // Cold path (once per stream): insert the stream and drain the
        // pending subscriptions under the manager lock.  Locking order is
        // always manager -> stream; Stream never calls back into this class.
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto [jdx, inserted]
            = mStreamsMap.try_emplace(streamIdentifier, std::move(stream));
        if (!inserted)
        {
            throw std::runtime_error("Failed to insert " + streamIdentifier
                                   + " into streams map");
        }
        Stream *newStream = jdx->second.get();
        // Whoever was subscribed to all is now subscribed to this stream
        for (const auto &contextAddress : mPendingSubscribeToAllRequests)
        {
            constexpr bool enqueueNextPacket{true};
            if (newStream->subscribe(contextAddress, enqueueNextPacket))
            {
                addToActiveSubscriptionsMap(contextAddress, streamIdentifier);
            }
            else
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Failed to subscribe {} to {}",
                                   contextAddress, streamIdentifier);
            }
        }
        // Whoever was particularly interested in this stream should be
        // subscribed; purge contexts whose every request is now filled
        for (auto it = mPendingSubscriptionRequests.begin();
             it != mPendingSubscriptionRequests.end();
             )
        {
            auto contextAddress = it->first;
            if (it->second.erase(streamIdentifier) == 1)
            {
                constexpr bool enqueueNextPacket{true};
                if (newStream->subscribe(contextAddress, enqueueNextPacket))
                {
                    addToActiveSubscriptionsMap(contextAddress,
                                                streamIdentifier);
                }
                else
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Failed to subscribe {} to {}",
                                       contextAddress, streamIdentifier);
                }
            }
            if (it->second.empty())
            {
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "All pending subscriptions filled for {}",
                                    std::to_string(contextAddress));
                it = mPendingSubscriptionRequests.erase(it);
            }
            else
            {
                ++it;
            }
        }
        }
    }

    /// Context is subscribing to set of streams
    void subscribe(
        uintptr_t contextAddress, 
        const std::vector<UDataPacketServiceAPI::V1::StreamIdentifier>
            &streamIdentifiers)
    {
        if (streamIdentifiers.empty()){return;}
        const std::lock_guard<std::mutex> lock(mMutex);
        for (const auto &identifier : streamIdentifiers)
        {
            auto streamIdentifier = Utilities::toName(identifier);
            auto idx = mStreamsMap.find(streamIdentifier);
            if (idx != mStreamsMap.end())
            {
                // Stream exists - add it
                try
                {
                    // I'm joining late
                    constexpr bool enqueueNextPacket{false};
                    if (idx->second->subscribe(contextAddress,
                                               enqueueNextPacket))
                    {
                        addToActiveSubscriptionsMap(contextAddress,
                                                    streamIdentifier);
                        SPDLOG_LOGGER_DEBUG(mLogger,
                                            "Subscribed {} to {}",
                                            std::to_string(contextAddress),
                                            streamIdentifier);
                    }
                    else
                    {
                        SPDLOG_LOGGER_DEBUG(mLogger,
                                            "Failed to subscribe {} to {}",
                                            std::to_string(contextAddress),
                                            streamIdentifier);
                    }
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                      "Failed to subscribe {} to {} because {}",
                                      std::to_string(contextAddress),
                                      streamIdentifier,
                                      std::string {e.what()});
                }
            }
            else
            {
                // Stream doesn't exist yet; note the pending subscription
                mPendingSubscriptionRequests[contextAddress]
                    .insert(streamIdentifier);
            }
        } // Loop on desired streams
        mNumberOfSubscribers = -1; // Reset for getNumberOfSubscribers()
    }

    /// Context is subscribe to all streams
    void subscribeToAll(uintptr_t contextAddress)
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        if (mPendingSubscribeToAllRequests.contains(contextAddress))
        {
            SPDLOG_LOGGER_INFO(mLogger,
                               "{} already waiting to subscribe to all",
                               std::to_string (contextAddress));
            return;
        }
        // Attach to all streams
        for (auto &stream : mStreamsMap)
        {
            auto streamIdentifier = stream.second->getIdentifier();
#ifndef NDEBUG
            assert(!streamIdentifier.empty());
#endif
            try
            {
                // I'm joining late - don't load packet that existed before me
                constexpr bool enqueueLatestPacket{false};
                if (stream.second->subscribe(contextAddress,
                                             enqueueLatestPacket))
                {
                    // Subscribed - add to active subscriptions
                    addToActiveSubscriptionsMap(contextAddress,
                                                streamIdentifier);

#ifndef NDEBUG
                    SPDLOG_LOGGER_DEBUG(mLogger,
                                        "{} subscribed to {}",
                                        std::to_string (contextAddress),
                                        streamIdentifier);
#endif
                }
                else
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "{} did not subscribe to {}",
                                       std::to_string (contextAddress),
                                       streamIdentifier);
                } 
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                   "{} failed to subscribe to {} because {}",
                                   std::to_string (contextAddress),
                                   streamIdentifier,
                                   std::string {e.what()});
            }
        }
        // And be ready for all future streams that come online
        mPendingSubscribeToAllRequests.insert(contextAddress);
        mNumberOfSubscribers = -1; // Reset for getNumberOfSubscribers()
    }

    [[nodiscard]] std::vector<UDataPacketServiceAPI::V1::Packet>
        getPackets(uintptr_t contextAddress) const
    {
        // Hot path: copy this context's stream pointers out under the lock,
        // then drain the streams outside it.  Streams are never removed so
        // the pointers remain valid.
        std::vector<Stream *> streams;
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mActiveSubscriptionsMap.find(contextAddress);
        if (idx == mActiveSubscriptionsMap.end()){return {};}
        streams.reserve(idx->second.size());
        for (const auto &streamIdentifier : idx->second)
        {
            const auto streamIndex = mStreamsMap.find(streamIdentifier);
            if (streamIndex != mStreamsMap.end())
            {
                streams.push_back(streamIndex->second.get());
            }
        }
        }
        std::vector<UDataPacketServiceAPI::V1::Packet> result;
        result.reserve(streams.size());
        for (auto *stream : streams)
        {
            auto packet = stream->getNextPacket(contextAddress);
            if (packet)
            {
                result.push_back(std::move(*packet));
            }
        }
        return result;
    }

    /// Context is leaving
    void unsubscribeFromAll(uintptr_t contextAddress)
    {
        bool wasUnsubscribed{false};
        // Purge the bookkeeping and collect the streams under the lock
        std::vector<Stream *> streams;
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        if (mPendingSubscriptionRequests.erase(contextAddress) == 1)
        {
            wasUnsubscribed = true;
        }
        if (mPendingSubscribeToAllRequests.erase(contextAddress) == 1)
        {
            wasUnsubscribed = true;
        }
        if (mActiveSubscriptionsMap.erase(contextAddress) == 1)
        {
            wasUnsubscribed = true;
        }
        mNumberOfSubscribers = -1; // Reset for getNumberOfSubscribers()
        // Unsubscribe from every stream, not just the active ones, in case
        // the bookkeeping ever drifts from the streams' subscriber maps
        streams.reserve(mStreamsMap.size());
        for (auto &stream : mStreamsMap)
        {
            streams.push_back(stream.second.get());
        }
        }
        // Then do the stream-by-stream work outside the manager lock
        for (auto *stream : streams)
        {
            try
            {
                if (stream->unsubscribe(contextAddress) ==
                    Stream::UnsubscribeResponse::Unsubscribed)
                {
                    wasUnsubscribed = true;
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                  "Failed to unsubscribe {} from {} because {}",
                                  std::to_string(contextAddress),
                                  stream->getIdentifier(),
                                  std::string {e.what()});
            }
        }
        if (wasUnsubscribed)
        {
            SPDLOG_LOGGER_DEBUG(mLogger,
                                "{} was unsubscribed from all",
                                std::to_string(contextAddress));
        }
        else
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} may not have been subscribed to anything",
                               std::to_string(contextAddress));
        }
    }

    /// @brief Gets the number of subscribers
    [[nodiscard]] int getNumberOfSubscribers() const noexcept
    {
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        // Early return?
        if (mNumberOfSubscribers >= 0){return mNumberOfSubscribers;}
        std::set<uintptr_t> allSubscribers;
        // Count them
        for (const auto &subscriber : mActiveSubscriptionsMap)
        {
            allSubscribers.insert(subscriber.first);
        }
        for (const auto &subscriber : mPendingSubscriptionRequests)
        {
            allSubscribers.insert(subscriber.first);
        }
        for (const auto &subscriber : mPendingSubscribeToAllRequests)
        {
            allSubscribers.insert(subscriber);
        }
        // Update and finish
        mNumberOfSubscribers = static_cast<int> (allSubscribers.size());
        return mNumberOfSubscribers;
        }
    }

    void unsubscribeAll()
    {
        // Do not let these get filled while I'm clearing
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        mNumberOfSubscribers =-1;
        mPendingSubscriptionRequests.clear();
        mPendingSubscribeToAllRequests.clear();
        // Purge the active subscriptions 
        for (auto &stream : mStreamsMap)
        {
             stream.second->unsubscribeAll();
        }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds {10});
        // Check
        auto newNumberOfSubscribers = getNumberOfSubscribers();
        if (newNumberOfSubscribers != 0)
        {
            SPDLOG_LOGGER_WARN(
               mLogger,
                "May not have purged all subscribers.  {} still remain.",
               newNumberOfSubscribers);
        }
    }

    /// Caller must hold mMutex
    void addToActiveSubscriptionsMap(uintptr_t contextAddress,
                                     const std::string &streamIdentifier)
    {
        mActiveSubscriptionsMap[contextAddress].insert(streamIdentifier);
    }
//private:
    SubscriptionManagerOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    // mMutex guards the four containers and the subscriber count below.
    // Locking order is manager then stream; a Stream method must never
    // call back into this class.
    mutable std::mutex mMutex;
    std::map
    <
        std::string,            // Stream identifier
        std::unique_ptr<Stream> // Stream (never removed; stable address)
    > mStreamsMap;
    std::map
    <
        uintptr_t,            // Context identifier
        std::set<std::string> // Stream identifiers
    > mActiveSubscriptionsMap;
    std::map
    <
        uintptr_t,
        std::set<std::string>
    > mPendingSubscriptionRequests;
    std::set<uintptr_t> mPendingSubscribeToAllRequests;
    StreamOptions mStreamOptions;
    mutable int mNumberOfSubscribers{-1};
};

SubscriptionManager::SubscriptionManager(
    const SubscriptionManagerOptions &options,
    std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<SubscriptionManagerImpl> (options,
                                                     std::move(logger)))
{
}

/// Add a packet
void SubscriptionManager::enqueuePacket(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto copy = packet;
    enqueuePacket(std::move(copy));
}

void SubscriptionManager::enqueuePacket(
    UDataPacketServiceAPI::V1::Packet &&packet)
{
    // Won't get far without this
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Stream identifier not set");
    }
    const auto &streamIdentifier = packet.stream_identifier();
    if (!streamIdentifier.has_network())
    {
        throw std::invalid_argument("Network not set");
    }
    if (!streamIdentifier.has_station())
    {
        throw std::invalid_argument("Station not set");
    }
    if (!streamIdentifier.has_channel())
    {
        throw std::invalid_argument("Channel not set");
    }
    if (!packet.has_number_of_samples())
    {
        throw std::invalid_argument("Number of samples not set");
    }
    if (packet.number_of_samples() < 1)
    {
        throw std::invalid_argument("No samples in packet");
    }
    if (packet.data_type() ==
        UDataPacketServiceAPI::V1::DataType::DATA_TYPE_UNKNOWN)
    {
        throw std::invalid_argument("Undefined data type");
    }
    if (!packet.has_sampling_rate())
    {
        throw std::invalid_argument("Sampling rate not set");
    }
    if (packet.sampling_rate() <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }   
    if (!packet.has_data())
    {
        throw std::invalid_argument("Data not set");
    }
    if (packet.data().empty())
    {
        throw std::invalid_argument("No data");
    }
    pImpl->enqueuePacket(std::move(packet));
}

/*
/// Subscribe to all
template<typename U>
void SubscriptionManager::subscribeToAll(U *serverContext)
{
    if (serverContext == nullptr)
    {
        throw std::invalid_argument("Server context is null");
    }
    auto contextAddress
        = reinterpret_cast<uintptr_t> (serverContext);
    subscribeToAll(contextAddress);
}
*/

void SubscriptionManager::subscribe(
    uintptr_t contextAddress,
    const std::vector<UDataPacketServiceAPI::V1::StreamIdentifier>
        &streamIdentifiersIn)
{
    if (streamIdentifiersIn.empty())
    {
        throw std::invalid_argument("No streams selected");
    }
    // Create a set of identifiers
    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streamIdentifiers;
    std::set<std::string> existingNames;
    for (const auto &identifier : streamIdentifiersIn)
    {
        auto thisName = Utilities::toName(identifier);
        if (!existingNames.contains(thisName))
        {
            streamIdentifiers.push_back(identifier);
            existingNames.insert(thisName);
        }
    }
    if (streamIdentifiers.empty())
    {
        throw std::runtime_error("Failed to create stream identifier list");
    }
    pImpl->subscribe(contextAddress, streamIdentifiers);
}


void SubscriptionManager::subscribeToAll(uintptr_t contextAddress)
{
    pImpl->subscribeToAll(contextAddress);
}

/*
template<typename U>
void SubscriptionManager::unsubscribeFromAll(U *serverContext)
{
    if (serverContext == nullptr)
    {
        throw std::invalid_argument("Server context is null");
    }
    auto contextAddress
        = reinterpret_cast<uintptr_t> (serverContext);
    return unsubscribeFromAll(contextAddress);
}
*/

void SubscriptionManager::unsubscribeFromAll(uintptr_t contextAddress)
{
    return pImpl->unsubscribeFromAll(contextAddress);
}

/// Gets the next packets
std::vector<UDataPacketServiceAPI::V1::Packet>
SubscriptionManager::getPackets(uintptr_t contextAddress) const
{
    return pImpl->getPackets(contextAddress);
}

/// Destructor
SubscriptionManager::~SubscriptionManager() = default;

/// Number of subscribers
int SubscriptionManager::getNumberOfSubscribers() const noexcept
{
    return pImpl->getNumberOfSubscribers();
}

/// Forcefully removes all subscxribers
void SubscriptionManager::unsubscribeAll()
{
    pImpl->unsubscribeAll();
}

///--------------------------------------------------------------------------///
///                            Template Instantiation                        ///
///--------------------------------------------------------------------------///
//template class
//UDataPacketService::SubscriptionManager<grpc::CallbackServerContext>;


