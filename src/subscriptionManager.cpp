#include <mutex>
#include <string>
#include <set>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <oneapi/tbb/concurrent_map.h>
#include <oneapi/tbb/concurrent_set.h>
#include <grpcpp/server.h>
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

import Utilities;

using namespace UDataPacketService;

class SubscriptionManager::SubscriptionManagerImpl
{
public:
    SubscriptionManagerImpl(const SubscriptionManagerOptions &options,
                            std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger),
        mStreamOptions(mOptions.getStreamOptions())
    {
    }
 
    /// Add packet (and, if it is a new stream, update subscribers)
    void enqueuePacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        auto streamIdentifier = Utilities::toName(packet);
        auto idx = mStreamsMap.find(streamIdentifier);
        if (idx != mStreamsMap.end())
        {
            try
            {
                idx->second->setNextPacket(std::move(packet));
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
        std::pair
        <
            std::string,
            std::unique_ptr<Stream>
        > newStream{streamIdentifier, std::move(stream)};
        auto [jdx, inserted] = mStreamsMap.insert(std::move(newStream));
        if (inserted)
        {
            auto streamIdentifier = jdx->second->getIdentifier();
            // Whoever was subscribed to all is not subscribed to this stream
            for (const auto &pendingSubscription : mPendingSubscribeToAllRequests)
            {
                auto contextAddress
                    = reinterpret_cast<uintptr_t> (pendingSubscription);
                constexpr bool enqueueNextPacket{true};
                if (jdx->second->subscribe(contextAddress, enqueueNextPacket))
                {
                    // Successful subscribe to all; add to active
                    // subscriptions 
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
            // Whoever was particularly interested in this stream should be
            // subscribed
            for (auto &pendingSubscription : mPendingSubscriptionRequests)
            {
                auto kdx = pendingSubscription.second.find(streamIdentifier);
                if (kdx != pendingSubscription.second.end())
                {
                    auto contextAddress
                        = reinterpret_cast<uintptr_t> (pendingSubscription.first);
                    constexpr bool enqueueNextPacket{true}; 
                    if (jdx->second->subscribe(contextAddress, enqueueNextPacket))
                    {
                        // Successful subscribe; add to the active subscriptions
                        addToActiveSubscriptionsMap(contextAddress,
                                                    streamIdentifier);
                    }
                    else
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                                           "Failed to subscribe {} to {}",
                                           contextAddress, streamIdentifier);
                    }
                    pendingSubscription.second.erase(streamIdentifier);
                } 
            }
            // If all of the subscriber's requests have been filled then purge
            // it from the pending list
            for (auto it = mPendingSubscriptionRequests.cbegin();
                 it != mPendingSubscriptionRequests.cend();
                 )
            {
                // This guy is fully subscribed
                if (it->second.empty())
                {
                    SPDLOG_LOGGER_DEBUG(mLogger,
                                        "All pending subscriptions filled for {}",
                                        std::to_string {it->first});
                    {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mPendingSubscriptionRequests.unsafe_erase(it++);
                    }
                }
                else
                {
                    ++it;
                }
            }
        }
        else
        {
            throw std::runtime_error("Failed to insert " + streamIdentifier
                                   + " into streams map");
        }
    }

    /// Context is subscribing to set of streams
    void subscribe(
        uintptr_t contextAddress, 
        const std::set<UDataPacketServiceAPI::V1::StreamIdentifier>
            &streamIdentifiers)
    {
        if (streamIdentifiers.empty()){return;}
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
                // Stream doesn't exist yet, add stream to pending subscriptions
                // Check our pending subscriptions for this context
                auto jdx = mPendingSubscriptionRequests.find(contextAddress);
                if (jdx != mPendingSubscriptionRequests.end())
                {
                    // The context already has this subscription pending
                    if (jdx->second.contains(streamIdentifier))
                    {
                        SPDLOG_LOGGER_DEBUG(mLogger,
                                  "{} already has a pending subscription for {}",
                                  std::to_string(contextAddress),
                                  streamIdentifier);
                    }
                    else
                    {
                        // Now it's pending
                        jdx->second.insert(streamIdentifier);
                    }
                }
                else
                {
                    // Need a new context with a new pending subscription
                    std::set<std::string> tempSet{streamIdentifier};
                    mPendingSubscriptionRequests.insert(
                        std::pair {contextAddress, std::move(tempSet)}
                    );
                }

            }
        } // Loop on desired streams
    }

    /// Context is subscribe to all streams
    void subscribeToAll(uintptr_t contextAddress)
    {
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
    }

    [[nodiscard]] std::vector<UDataPacketServiceAPI::V1::Packet>
        getPackets(uintptr_t contextAddress) const
    {
        std::vector<UDataPacketServiceAPI::V1::Packet> result;
        // Look through my active subscriptions
        for (const auto &activeSubscription : mActiveSubscriptionsMap)
        {
            for (const auto &streamIdentifier : activeSubscription.second)
            {
                const auto streamIndex = mStreamsMap.find(streamIdentifier);
                if (streamIndex != mStreamsMap.end())
                {
                    try
                    {
                        auto packet
                            = streamIndex->second->getNextPacket(contextAddress);
                        if (packet)
                        {
                            result.push_back(std::move(*packet));
                        }
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                        "Failed to get packet from stream {} for {} because {}",
                          streamIndex->first,
                          std::to_string(contextAddress),
                          std::string {e.what()});
                    }
                }
                else
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                          "Failed to find stream {} for active subscriber {}",
                          streamIndex->first, std::to_string(contextAddress));
                }
            }
        }
        return result;
    }

    /// Context is leaving
    void unsubscribeFromAll(uintptr_t contextAddress)
    {
        bool wasUnsubscribed{false};
        // Pop from the pending fine-grained requests
        size_t erased = mPendingSubscriptionRequests.unsafe_erase(contextAddress);
        if (erased == 1){wasUnsubscribed = true;}
        // Pop from the pending subscribe to all requests
        erased = mPendingSubscribeToAllRequests.unsafe_erase(contextAddress);
        if (erased == 1){wasUnsubscribed = true;}
        // Pop from the active subscriptions 
        bool purgedFromActiveSubscriptions{false};
        for (auto &stream : mStreamsMap)
        {
            try
            {
                if (!stream.second->unsubscribe(contextAddress))
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Did not unsubscribe {} from {}",
                                       std::to_string(contextAddress),
                                       stream.first);
                }
                else
                {
                    wasUnsubscribed = true;
                    if (!purgedFromActiveSubscriptions)
                    {
                        mActiveSubscriptionsMap.unsafe_erase(contextAddress);
                        purgedFromActiveSubscriptions = true; 
                    }
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                  "Failed to unsubscribe {} from {} because {}",
                                  std::to_string(contextAddress),
                                  stream.first,
                                  std::string {e.what()});
            }
        }
        // Update number of subscribers 
        {
        std::lock_guard<std::mutex> lock(mMutex);
        mNumberOfSubscribers =-1; // Reset for getNumberOfSubscribers()
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
        std::lock_guard<std::mutex> lock(mMutex);
        if (mNumberOfSubscribers < 0)
        {
            std::set<uintptr_t> allSubscribers;
            for (const auto &stream : mStreamsMap)
            {
                auto subscribers = stream.second->getSubscribers();
                allSubscribers.insert(subscribers.begin(), subscribers.end());
            }
            if (allSubscribers.empty())
            {
                mNumberOfSubscribers
                   = mPendingSubscriptionRequests.size()
                   + mPendingSubscribeToAllRequests.size();
            }
            else
            {
                mNumberOfSubscribers = static_cast<int> (allSubscribers.size());
            }
        }
        return mNumberOfSubscribers;
        }
    }

    void addToActiveSubscriptionsMap(uintptr_t contextAddress,
                                     const std::string &streamIdentifier)
    {
        if (mActiveSubscriptionsMap.contains(contextAddress))
        {
            mActiveSubscriptionsMap[contextAddress].insert(streamIdentifier);
        }
        else
        {
            std::set<std::string> newSet{streamIdentifier};
            mActiveSubscriptionsMap[contextAddress] = std::move(newSet);
        }
    }
//private:
    SubscriptionManagerOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    mutable std::mutex mMutex;
    oneapi::tbb::concurrent_map
    <
        std::string,            // Stream identifier
        std::unique_ptr<Stream> // Stream
    > mStreamsMap;
    oneapi::tbb::concurrent_map
    <
        uintptr_t,            // Context identifier
        std::set<std::string> // Stream identifiers
    > mActiveSubscriptionsMap;
    oneapi::tbb::concurrent_map
    <
        uintptr_t, //T *, //grpc::CallbackServerContext *,
        std::set<std::string>
    > mPendingSubscriptionRequests;
    oneapi::tbb::concurrent_set
    <
        uintptr_t //T * //grpc::CallbackServerContext *
    > mPendingSubscribeToAllRequests;
    StreamOptions mStreamOptions;
    mutable int mNumberOfSubscribers{-1};
};

SubscriptionManager::SubscriptionManager(
    const SubscriptionManagerOptions &options,
    std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<SubscriptionManagerImpl> (options, logger))
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
    if (!packet.has_number_of_samples())
    {
        throw std::invalid_argument("Number of samples not set");
    }
    if (packet.data_type() ==
        UDataPacketServiceAPI::V1::DataType::DATA_TYPE_UNKNOWN)
    {
        throw std::invalid_argument("Undefined data type");
    }
    if (packet.sampling_rate() <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }   
    if (!packet.has_data())
    {
        throw std::invalid_argument("Data not set");
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
    const std::set<UDataPacketServiceAPI::V1::StreamIdentifier>
        &streamIdentifiers)
{
    if (streamIdentifiers.empty())
    {
        throw std::invalid_argument("No streams selected");
    }
    pImpl->subscribe(contextAddress, streamIdentifiers);
}


void SubscriptionManager::subscribeToAll(uintptr_t contextAddress)
{
    pImpl->subscribeToAll(contextAddress);
}


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

///--------------------------------------------------------------------------///
///                            Template Instantiation                        ///
///--------------------------------------------------------------------------///
//template class
//UDataPacketService::SubscriptionManager<grpc::CallbackServerContext>;


