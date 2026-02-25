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
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

import Utilities;

using namespace UDataPacketService;

template<typename T>
class SubscriptionManager<T>::SubscriptionManagerImpl
{
public:
    // Add packet (and, if it is a new stream, update subscribers)
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
            // Whoever was subscribed to all is not subscribed to this stream
            for (const auto &pendingSubscription : mPendingSubscribeToAllRequests)
            {
                auto contextAddress
                    = reinterpret_cast<uintptr_t> (pendingSubscription);
                constexpr bool enqueueNextPacket{true};
                if (!jdx->second->subscribe(contextAddress, enqueueNextPacket))
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
                    if (!jdx->second->subscribe(contextAddress, enqueueNextPacket))
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
                                        it->first->peer());
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
    /// Client unsubscribes from all streams
    void unsubscribeFromAll(T *context)
    {
        bool wasUnsubscribed{false};
        if (context == nullptr)
        {
            throw std::invalid_argument("Context is null");
        }
        auto contextAddress = reinterpret_cast<uintptr_t> (context);
        auto peer = context->peer();
        // Pop from the pending fine-grained requests
        size_t erased = mPendingSubscriptionRequests.unsafe_erase(context);
        if (erased == 1){wasUnsubscribed = true;}
        // Pop from the pending subscribe to all requests
        erased = mPendingSubscribeToAllRequests.unsafe_erase(context);
        if (erased == 1){wasUnsubscribed = true;}
        // Pop from the active subscriptions 
        for (auto &stream : mStreamsMap)
        {
            try
            {
                if (!stream.second->unsubscribe(contextAddress))
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Did not unsubscribe {} from {}",
                                       peer,
                                       stream.first);
                }
                else
                {
                    wasUnsubscribed = true;
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                  "Failed to unsubscribe {} from {} because {}",
                                  peer,
                                  stream.first,
                                  std::string {e.what()});
            }
        }
        {
        std::lock_guard<std::mutex> lock(mMutex);
        mNumberOfSubscribers =-1; // Reset for getNumberOfSubscribers()
        }
        if (wasUnsubscribed)
        {
            SPDLOG_LOGGER_DEBUG(mLogger,
                                "{} was unsubscribed from all", 
                                peer);
        }
        else
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} may not have been subscribed to anything",
                               peer);
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

//private:
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    mutable std::mutex mMutex;
    oneapi::tbb::concurrent_map
    <
        std::string,
        std::unique_ptr<Stream>
    > mStreamsMap;
    oneapi::tbb::concurrent_map
    <
        T *, //grpc::CallbackServerContext *,
        std::set<std::string>
    > mPendingSubscriptionRequests;
    oneapi::tbb::concurrent_set
    <
        T * //grpc::CallbackServerContext *
    > mPendingSubscribeToAllRequests;
    StreamOptions mStreamOptions;
    mutable int mNumberOfSubscribers{-1};
};

template<typename T>
SubscriptionManager<T>::SubscriptionManager() :
    pImpl(std::make_unique<SubscriptionManagerImpl> ())
{
}

/// Add a packet
template<typename T>
void SubscriptionManager<T>::enqueuePacket(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto copy = packet;
    enqueuePacket(std::move(copy));
}

template<typename T>
void SubscriptionManager<T>::enqueuePacket(
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


/// Subscribe to all
template<typename T>
void SubscriptionManager<T>::subscribeToAll(T *serverContext)
{
    if (serverContext == nullptr)
    {
        throw std::invalid_argument("Server context is null");
    }
    //pImpl->subscribeToAll(context);
}


template<typename T>
void SubscriptionManager<T>::unsubscribeFromAll(T *serverContext)
{
    if (serverContext == nullptr)
    {
        throw std::invalid_argument("Server context is null");
    }
    return pImpl->unsubscribeFromAll(serverContext);
}

/// Destructor
template<typename T>
SubscriptionManager<T>::~SubscriptionManager() = default;

///--------------------------------------------------------------------------///
///                            Template Instantiation                        ///
///--------------------------------------------------------------------------///
template class
UDataPacketService::SubscriptionManager<grpc::CallbackServerContext>;


