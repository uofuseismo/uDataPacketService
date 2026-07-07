module;
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifndef NDEBUG
#include <cassert>
#endif
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include "uDataPacketService/metricsSingleton.hpp"
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketService/utilities.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/data_type.pb.h"



export module AsyncWriter;

namespace
{
#ifndef NDEBUG
void checkPacket(const UDataPacketServiceAPI::V1::Packet &packet)
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("No stream identifier set");
    }
    const auto &identifier = packet.stream_identifier();
    if (!identifier.has_network())
    {
        throw std::invalid_argument("Network not set");
    }
    if (!identifier.has_station())
    {
        throw std::invalid_argument("Station not set");
    }
    if (!identifier.has_channel())
    {
        throw std::invalid_argument("Channel not set");
    }
    if (!packet.has_start_time())
    {
        throw std::invalid_argument("No start time set");
    }
    if (!packet.has_sampling_rate())
    {
        throw std::invalid_argument("No sampling rate set");
    }
    if (packet.sampling_rate() <= 0)
    {
        throw std::invalid_argument("Sampling not positive");
    }
    if (!packet.has_number_of_samples())
    {
        throw std::invalid_argument("No samples set");
    }
    if (packet.number_of_samples() < 1)
    { 
        throw std::invalid_argument("No samples");
    }
    if (!packet.has_data_type())
    {
        throw std::invalid_argument("No data type set");
    }
    if (!packet.has_data())
    {
        throw std::invalid_argument("No data set");
    } 
    if (packet.data().empty())
    {
        throw std::invalid_argument("Data empty");
    }
}
#endif
}

namespace UDataPacketService
{

[[nodiscard]]
bool validateSubscriber(const grpc::CallbackServerContext *context,
                        const std::string &accessToken)
{
    if (accessToken.empty()){return true;}
    for (const auto &item : context->client_metadata())
    {   
        if (item.first == "x-custom-auth-token")
        {
            if (item.second == accessToken)
            {
                return true;
            }
        }
    }   
    return false;
}

///--------------------------------------------------------------------------///
///                               Subscribe                                  ///
///--------------------------------------------------------------------------///

export
class Subscribe :
    public grpc::ServerWriteReactor<UDataPacketServiceAPI::V1::Packet>
{
public:
    Subscribe
    (
        grpc::CallbackServerContext *context,
        const UDataPacketServiceAPI::V1::SubscriptionRequest *request,
        const ServerOptions &serverOptions,
        const bool isSecureConnection,
        std::shared_ptr
        <
           UDataPacketService::SubscriptionManager
        > subscriptionManager,
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :
        mContext(context),
        mContextAddress(reinterpret_cast<uintptr_t> (mContext)),
        mOptions(serverOptions),
        mSubscriptionManager(std::move(subscriptionManager)),
        mLogger(std::move(logger)),
        mKeepRunning(keepRunning)
    {
        mPeer = mContext->peer();
        if (request)
        {
            if (!request->identifier().empty())
            {
                mPeer = mPeer + " (" + request->identifier() + ")";
            }
        }

        // Authenticate
        if (isSecureConnection &&
            mOptions.getGRPCOptions().getAccessToken() != std::nullopt)
        {
            auto accessToken
                = *mOptions.getGRPCOptions().getAccessToken();
            if (!validateSubscriber(mContext, accessToken))
            {
                SPDLOG_LOGGER_INFO(mLogger, "Rejected {}", mPeer);
                const grpc::Status status{grpc::StatusCode::UNAUTHENTICATED,
R"""(
Subscriber must provide access token in x-custom-auth-token header field.
)"""};
                Finish(status);
                return;
            }
            else
            {
                SPDLOG_LOGGER_INFO(mLogger, "Validated {}", mPeer);
            }
        }
        else
        {
            SPDLOG_LOGGER_INFO(mLogger, "{} connected to subscribe RPC", mPeer);
        }

        auto maximumNumberOfSubscribers
            = mOptions.getMaximumNumberOfSubscribers();
        if (mSubscriptionManager->getNumberOfSubscribers() >=
            maximumNumberOfSubscribers)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                "Subscribe RPC rejecting {} because max number of subscribers hit",
                 mPeer);
            const grpc::Status status{grpc::StatusCode::RESOURCE_EXHAUSTED,
                                "Max subscribers hit - try again later"};
            Finish(status);
            return;
       }

        // Allow client to subscribe
        try
        {
            if (request->selections().empty())
            {
                const grpc::Status status{grpc::StatusCode::INVALID_ARGUMENT,
                                    "No streams specified - check your selections."};
                Finish(status);
                return;
            }
            std::vector<UDataPacketServiceAPI::V1::StreamIdentifier>
                streamSelections;
            std::set<std::string> existingIdentifiers;
            for (const auto &selector : request->selections())
            {
                std::string name;
                try
                {
                    name = Utilities::toName(selector);
                }
                catch (...)
                {
                    const grpc::Status status{grpc::StatusCode::INVALID_ARGUMENT,
                                        "Invalid selection format.  A network, station, and channel is required"}; 
                    Finish(status);
                    return;
                }
                if (!existingIdentifiers.contains(name))
                {
                    SPDLOG_LOGGER_INFO(mLogger, "{} will subscribe to {}",
                                       mPeer, name);
                    streamSelections.push_back(selector);
                    existingIdentifiers.insert(name);
                }
            }
            // No streams after all this?
            if (streamSelections.empty())
            {
                SPDLOG_LOGGER_WARN(mLogger, "Could not create streams");
                const grpc::Status status{grpc::StatusCode::INVALID_ARGUMENT,
                       "No streams created.  Verify your stream selections."};
                Finish(status);
                return;
            }
            SPDLOG_LOGGER_INFO(mLogger,
                               "Subscribing {} to {} streams",
                               mPeer, streamSelections.size());
            mSubscriptionManager->subscribe(mContextAddress, streamSelections);
            mSubscribed.store(true);
            auto nSubscribers = mSubscriptionManager->getNumberOfSubscribers();
            auto utilization
                = static_cast<double> (nSubscribers)
                 /std::max(1, maximumNumberOfSubscribers);
            mMetrics.updateUtilization(utilization);
            SPDLOG_LOGGER_INFO(mLogger,
                          "Now managing {} subscribers.  Resource {} pct utilized.",
                          nSubscribers, utilization*100.0);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} failed to subscribe because {}",
                               mPeer, std::string {e.what()});
            Finish(grpc::Status(grpc::StatusCode::INTERNAL,
                                "Failed to subscribe"));
            return;
        }
        // Start
        SPDLOG_LOGGER_DEBUG(mLogger, "Subscribe RPC for {} is starting",
                            mPeer);
        mMaximumPollInterval = mOptions.getMaximumWriterPollInterval();
        pump();
    }

    void OnWriteDone(bool ok) override
    {
        if (!ok)
        {
            if (mContext && mContext->IsCancelled())
            {
                return finishUp(grpc::Status::CANCELLED);
            }
            return finishUp(grpc::Status(grpc::StatusCode::UNKNOWN,
                                         "Unexpected failure"));
        }
        // Packet is flushed; can now safely purge the element to write
        mPacketsQueue.pop();
        // Start next write
        pump();
    }

    // This needs to perform quickly.  I should do blocking work but
    // this is my last ditch effort to evict the context from the 
    // subscription manager..
    void OnDone() override
    {
        if (mSubscribed.load())
        {
            mSubscriptionManager->unsubscribeFromAll(mContextAddress);
            mSubscribed.store(false);
        }
        auto maximumNumberOfSubscribers
            = mOptions.getMaximumNumberOfSubscribers();
        auto nSubscribers = mSubscriptionManager->getNumberOfSubscribers();
            auto utilization
                = static_cast<double> (nSubscribers)
                 /std::max(1, maximumNumberOfSubscribers);
        SPDLOG_LOGGER_INFO(mLogger,
            "Subscribe RPC completed for {}.  Subscription manager is now managing {} subscribers.  Resource {} pct utilized.",
            mPeer, 
            std::to_string(nSubscribers),
            utilization*100.0);
        delete this;
    }

    void OnCancel() override
    {
        SPDLOG_LOGGER_INFO(mLogger,
                           "Subscribe RPC cancelled for {}.",
                           mPeer);
        // Wake the pump: a pending alarm fires immediately with ok=false
        // and a pending write completes with ok=false via OnWriteDone.
        // Either way the pump sees the cancel and finishes up.
        mAlarm.Cancel();
    }

#ifndef NDEBUG
    ~Subscribe()
    {
        SPDLOG_LOGGER_INFO(mLogger, "In destructor");
    }
#endif
//private:
    // The write pump.  Exactly one continuation is outstanding at any
    // instant - an in-flight StartWrite (resumes in OnWriteDone) or an
    // armed alarm (resumes in the alarm callback) - so the pump is
    // logically single threaded and holds no thread while idle.  Finish
    // is only ever called from the pump, so when OnDone runs nothing can
    // still be pending and delete this is safe.
    void pump()
    {
        // Server shutting down or client gone?
        if (!mKeepRunning->load() || mContext->IsCancelled())
        {
            if (mContext->IsCancelled())
            {
                SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of client side cancel",
                    mPeer);
                return finishUp(grpc::Status::CANCELLED);
            }
            SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of server side cancel",
                mPeer);
            return finishUp(grpc::Status::OK);
        }

        // Try to get more packets to write
        if (mPacketsQueue.empty())
        {
            try
            {
                auto packetsBuffer
                     = mSubscriptionManager->getPackets(mContextAddress);
                for (auto &packet : packetsBuffer)
                {
                    const bool allow{true}; // TODO check packet at some point?
#ifndef NDEBUG
                    try
                    {
                        checkPacket(packet);
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                           "Skipping invalid packet: ({})",
                           std::string {e.what()});
                        continue;
                    }
#endif
                    if (mCheckPackets)
                    {
                    }
                    if (!allow){continue;}
                    if (mPacketsQueue.size() > mMaximumQueueSize)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                           "RPC writer queue exceeded for {} - popping element",
                           mPeer);
                        mPacketsQueue.pop();
                     }
                     mPacketsQueue.push(std::move(packet));
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Failed to get next packet for {} because {}",
                                   mPeer,
                                   std::string {e.what()});
            }
        }

        // Data to send: put the front packet on the wire.  The pump
        // resumes in OnWriteDone.
        if (!mPacketsQueue.empty())
        {
            mCurrentPollInterval = mPollInterval; // Data is flowing again
            mMetrics.incrementSentPacketsCounter();
            StartWrite(&mPacketsQueue.front());
            return;
        }

        // Idle: hand the thread back; the alarm resumes the pump at the
        // deadline (ok=true) or immediately on Cancel (ok=false).  While
        // the stream stays quiet back off towards the maximum, which the
        // options keep below the server shutdown deadline.
        const auto interval
            = std::min(mCurrentPollInterval, mMaximumPollInterval);
        mCurrentPollInterval = std::min(interval*2, mMaximumPollInterval);
        mAlarm.Set(std::chrono::system_clock::now() + interval,
                   [this](bool){pump();});
    }

    void finishUp(const grpc::Status &status)
    {
        if (mSubscribed.load())
        {
            mSubscriptionManager->unsubscribeFromAll(mContextAddress);
            mSubscribed.store(false);
        }
        Finish(status);
    }

    grpc::CallbackServerContext *mContext{nullptr};
    uintptr_t mContextAddress;
    ServerOptions mOptions;
    std::shared_ptr
    <
        UDataPacketService::SubscriptionManager
    > mSubscriptionManager{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> *mKeepRunning{nullptr};
    UDataPacketService::Metrics::MetricsSingleton &mMetrics
    {
        UDataPacketService::Metrics::MetricsSingleton::getInstance()
    };
    grpc::Alarm mAlarm;
    std::string mPeer;
    size_t mMaximumQueueSize{2048};
    std::queue<UDataPacketServiceAPI::V1::Packet> mPacketsQueue;
    std::chrono::milliseconds mPollInterval{20};
    std::chrono::milliseconds mCurrentPollInterval{mPollInterval};
    std::chrono::milliseconds mMaximumPollInterval{250};
    std::atomic<bool> mSubscribed{false};
    bool mCheckPackets{false};
};

///--------------------------------------------------------------------------///
///                            Subscribe to All                              ///
///--------------------------------------------------------------------------///

export 
class SubscribeToAll :
    public grpc::ServerWriteReactor<UDataPacketServiceAPI::V1::Packet>
{
public:
    SubscribeToAll
    (       
        grpc::CallbackServerContext *context,
        const UDataPacketServiceAPI::V1::SubscribeToAllRequest *request,
        const ServerOptions &serverOptions,
        const bool isSecureConnection,
        std::shared_ptr
        <
           UDataPacketService::SubscriptionManager
        > subscriptionManager,
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :     
        mContext(context),
        mContextAddress(reinterpret_cast<uintptr_t> (mContext)),
        mOptions(serverOptions),
        mSubscriptionManager(std::move(subscriptionManager)),
        mLogger(std::move(logger)),
        mKeepRunning(keepRunning)
    {   
        mPeer = mContext->peer();
        if (request)
        {
            if (!request->identifier().empty())
            {
                mPeer = mPeer + " (" + request->identifier() + ")";
            }
        }

        // Authenticate
        if (isSecureConnection &&
            mOptions.getGRPCOptions().getAccessToken() != std::nullopt)
        {
            auto accessToken
                = *mOptions.getGRPCOptions().getAccessToken();
            if (!validateSubscriber(mContext, accessToken))
            {
                SPDLOG_LOGGER_INFO(mLogger, "Rejected {}", mPeer);
                const grpc::Status status{grpc::StatusCode::UNAUTHENTICATED,
R"""(
Subscriber must provide access token in x-custom-auth-token header field.
)"""};
                Finish(status);
                return;
            }
            else
            {
                SPDLOG_LOGGER_INFO(mLogger, "Validated {}", mPeer);
            }
        }
        else
        {
            SPDLOG_LOGGER_INFO(mLogger, "{} connected to subscribe to all RPC.", mPeer);
        }

        // Resource exhausted?
        auto maximumNumberOfSubscribers
            = mOptions.getMaximumNumberOfSubscribers();
        if (mSubscriptionManager->getNumberOfSubscribers() >=
            maximumNumberOfSubscribers)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                "Subscribe to all RPC rejecting {} because max number of subscribers hit.",
                 mPeer);
            const grpc::Status status{grpc::StatusCode::RESOURCE_EXHAUSTED,
                                "Max subscribers hit - try again later"};
            Finish(status);
            return;
       }

        // Allow client to subscribe
        try
        {
            SPDLOG_LOGGER_INFO(mLogger,
                               "Subscribing {} to all streams",
                               mPeer);
            mSubscriptionManager->subscribeToAll(mContextAddress);
            mSubscribed = true;
            auto nSubscribers = mSubscriptionManager->getNumberOfSubscribers();
            auto utilization
                = static_cast<double> (nSubscribers)
                 /std::max(1, maximumNumberOfSubscribers);
            mMetrics.updateUtilization(utilization);
            SPDLOG_LOGGER_INFO(mLogger,
                          "Now managing {} subscribers.  Resource {} pct utilized.",
                          nSubscribers, utilization*100.0);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} failed to subscribe because {}",
                               mPeer, std::string {e.what()});
            Finish(grpc::Status(grpc::StatusCode::INTERNAL,
                                "Failed to subscribe"));
            return;
        }
        // Start
        mMaximumPollInterval = mOptions.getMaximumWriterPollInterval();
        pump();
    }

    void OnWriteDone(bool ok) override
    {
        if (!ok)
        {
            if (mContext && mContext->IsCancelled())
            {
                return finishUp(grpc::Status::CANCELLED);
            }
            return finishUp(grpc::Status(grpc::StatusCode::UNKNOWN,
                                         "Unexpected failure"));
        }
        // Packet is flushed; can now safely purge the element to write
        mPacketsQueue.pop();
        // Start next write
        pump();
    }

    // This needs to perform quickly.  I should do blocking work but
    // this is my last ditch effort to evict the context from the 
    // subscription manager..
    void OnDone() override
    {
        if (mSubscribed)
        {
            mSubscriptionManager->unsubscribeFromAll(mContextAddress);
            mSubscribed = false;
        }
        auto maximumNumberOfSubscribers
            = mOptions.getMaximumNumberOfSubscribers();
        auto nSubscribers = mSubscriptionManager->getNumberOfSubscribers();
            auto utilization
                = static_cast<double> (nSubscribers)
                 /std::max(1, maximumNumberOfSubscribers);
        SPDLOG_LOGGER_INFO(mLogger,
            "Subscribe to all RPC completed for {}.  Subscription manager is now managing {} subscribers.  Resource {} pct utilized.",
            mPeer,
            std::to_string(nSubscribers),
            utilization*100.0);
        delete this;
    }

    void OnCancel() override
    {
        SPDLOG_LOGGER_INFO(mLogger,
                           "Subscribe to all RPC cancelled for {}.",
                           mPeer);
        // Wake the pump: a pending alarm fires immediately with ok=false
        // and a pending write completes with ok=false via OnWriteDone.
        // Either way the pump sees the cancel and finishes up.
        mAlarm.Cancel();
    }

#ifndef NDEBUG
    ~SubscribeToAll()
    {
        SPDLOG_LOGGER_INFO(mLogger, "In destructor");
    }
#endif

//private:
    // The write pump.  Exactly one continuation is outstanding at any
    // instant - an in-flight StartWrite (resumes in OnWriteDone) or an
    // armed alarm (resumes in the alarm callback) - so the pump is
    // logically single threaded and holds no thread while idle.  Finish
    // is only ever called from the pump, so when OnDone runs nothing can
    // still be pending and delete this is safe.
    void pump()
    {
        // Server shutting down or client gone?
        if (!mKeepRunning->load() || mContext->IsCancelled())
        {
            if (mContext->IsCancelled())
            {
                SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of client side cancel",
                    mPeer);
                return finishUp(grpc::Status::CANCELLED);
            }
            SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of server side cancel",
                mPeer);
            return finishUp(grpc::Status::OK);
        }

        // Try to get more packets to write
        if (mPacketsQueue.empty())
        {
            try
            {
                auto packetsBuffer
                     = mSubscriptionManager->getPackets(mContextAddress);
                for (auto &packet : packetsBuffer)
                {
                    const bool allow{true}; // TODO actually check packets?
#ifndef NDEBUG
                    try
                    {
                        checkPacket(packet);
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                           "Skipping invalid packet: ({})",
                           std::string {e.what()});
                        continue;
                    }
#endif
                    if (mCheckPackets)
                    {
                    }
                    if (!allow){continue;}
                    if (mPacketsQueue.size() > mMaximumQueueSize)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                           "RPC writer queue exceeded for {} - popping element",
                           mPeer);
                        mPacketsQueue.pop();
                     }
                     mPacketsQueue.push(std::move(packet));
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Failed to get next packet for {} because {}",
                                   mPeer,
                                   std::string {e.what()});
            }
        }

        // Data to send: put the front packet on the wire.  The pump
        // resumes in OnWriteDone.
        if (!mPacketsQueue.empty())
        {
            mCurrentPollInterval = mPollInterval; // Data is flowing again
            mMetrics.incrementSentPacketsCounter();
            StartWrite(&mPacketsQueue.front());
            return;
        }

        // Idle: hand the thread back; the alarm resumes the pump at the
        // deadline (ok=true) or immediately on Cancel (ok=false).  While
        // the stream stays quiet back off towards the maximum, which the
        // options keep below the server shutdown deadline.
        const auto interval
            = std::min(mCurrentPollInterval, mMaximumPollInterval);
        mCurrentPollInterval = std::min(interval*2, mMaximumPollInterval);
        mAlarm.Set(std::chrono::system_clock::now() + interval,
                   [this](bool){pump();});
    }

    void finishUp(const grpc::Status &status)
    {
        if (mSubscribed.load())
        {
            mSubscriptionManager->unsubscribeFromAll(mContextAddress);
            mSubscribed.store(false);
        }
        Finish(status);
    }

    grpc::CallbackServerContext *mContext{nullptr};
    uintptr_t mContextAddress;
    ServerOptions mOptions;
    std::shared_ptr
    <
        UDataPacketService::SubscriptionManager
    > mSubscriptionManager{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> *mKeepRunning{nullptr};
    UDataPacketService::Metrics::MetricsSingleton &mMetrics
    {
        UDataPacketService::Metrics::MetricsSingleton::getInstance()
    };
    grpc::Alarm mAlarm;
    std::string mPeer;
    size_t mMaximumQueueSize{2048};
    std::queue<UDataPacketServiceAPI::V1::Packet> mPacketsQueue;
    std::chrono::milliseconds mPollInterval{10};
    std::chrono::milliseconds mCurrentPollInterval{mPollInterval};
    std::chrono::milliseconds mMaximumPollInterval{250};
    std::atomic<bool> mSubscribed{false};
    bool mCheckPackets{false};
};

}
