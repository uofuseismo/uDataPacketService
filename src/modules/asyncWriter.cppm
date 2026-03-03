module;
#include <string>
#include <queue>
#include <vector>
#include <set>
#include <atomic>
#ifndef NDEBUG
#include <cassert>
#endif
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/subscriberOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"


export module AsyncWriter;
import Utilities;

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
        const SubscriberOptions &subscriberOptions,
        const bool isSecure,
        std::shared_ptr
        <
           UDataPacketService::SubscriptionManager
        > subscriptionManager,
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :
        mContext(context),
        mContextAddress(reinterpret_cast<uintptr_t> (mContext)),
        mOptions(subscriberOptions),
        mSubscriptionManager(subscriptionManager),
        mLogger(logger),
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
        if (isSecure &&
            mOptions.getGRPCServerOptions().getAccessToken() != std::nullopt)
        {
            auto accessToken
                = *mOptions.getGRPCServerOptions().getAccessToken();
            if (!validateSubscriber(mContext, accessToken))
            {
                SPDLOG_LOGGER_INFO(mLogger, "Rejected {}", mPeer);
                grpc::Status status{grpc::StatusCode::UNAUTHENTICATED,
R"""(
Subscriber must provide access token in x-custom-auth-token header field.
)"""};
                Finish(status);
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

int maximumNumberOfSubscribers{8};
/*
        auto maximumNumberOfSubscribers
            = mOptions.getMaximumNumberOfSubscribers();
*/
        if (mSubscriptionManager->getNumberOfSubscribers() >=
            maximumNumberOfSubscribers)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                "Subscribe RPC rejecting {} because max number of subscribers hit",
                 mPeer);
            grpc::Status status{grpc::StatusCode::RESOURCE_EXHAUSTED,
                                "Max subscribers hit - try again later"};
            Finish(status);
       }

        // Subscribe
        try
        {
            if (request->selections().empty())
            {
                grpc::Status status{grpc::StatusCode::INVALID_ARGUMENT,
                                    "No streams specified - check your selections."};
                Finish(status);
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
                    grpc::Status status{grpc::StatusCode::INVALID_ARGUMENT,
                                        "Invalid selection format.  A network, station, and channel is required"}; 
                    Finish(status);
                }
                if (!existingIdentifiers.contains(name))
                {
                    streamSelections.push_back(selector);
                    existingIdentifiers.insert(name);
                }
            }
            SPDLOG_LOGGER_INFO(mLogger,
                               "Subscribing {} to {} streams",
                               mPeer, streamSelections.size());
            mSubscriptionManager->subscribe(mContextAddress, streamSelections);
            mSubscribed = true;
            auto nSubscribers = mSubscriptionManager->getNumberOfSubscribers();
            auto utilization
                = static_cast<double> (nSubscribers)
                 /std::max(1, maximumNumberOfSubscribers);
//            mMetrics.updateSubscriberUtilization(utilization);
            SPDLOG_LOGGER_INFO(mLogger,
                          "Now managing {} subscribers (Resource {} pct utilized)",
                          nSubscribers, utilization*100.0);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} failed to subscribe because {}",
                               mPeer, std::string {e.what()});
            Finish(grpc::Status(grpc::StatusCode::INTERNAL,
                                "Failed to subscribe"));
        }
        // Start
        nextWrite();
    }

    // This needs to perform quickly.  I should do blocking work but
    // this is my last ditch effort to evict the context from the 
    // subscription manager..
    void OnDone() override
    {
        if (mContext)
        {   
            if (!mSubscribed)
            {   
                mSubscriptionManager->unsubscribeFromAll(mContextAddress);
                mSubscribed = false;
            }   
        }   
        auto nSubscribers = mSubscriptionManager->getNumberOfSubscribers();
        SPDLOG_LOGGER_INFO(mLogger,
            "Subscribe RPC completed for {}.  Subscription manager is now managing {} subscribers.",
            mPeer, 
            std::to_string(nSubscribers));
        delete this;
    }

    void OnCancel() override
    {
        SPDLOG_LOGGER_INFO(mLogger,
                           "Subscribe RPC cancelled for {}.",
                           mPeer);
        if (mSubscribed)
        {
            mSubscriptionManager->unsubscribeFromAll(mContextAddress);
            mSubscribed = false;
        }
    }

#ifndef NDEBUG
    ~Subscribe()
    {
        SPDLOG_LOGGER_INFO(mLogger, "In destructor");
    }   
#endif
//private:
    void nextWrite()
    {
        // Keep running either until the server or client quits
        while (mKeepRunning->load())
        {
            // Cancel means we leave now
            if (mContext->IsCancelled()){break;}

            // Get any remaining packets on the queue on the wire
            if (!mPacketsQueue.empty() && !mWriteInProgress)
            {
                const auto &packet = mPacketsQueue.front();
                mWriteInProgress = true;
                StartWrite(&packet);
                return;
            }

            // Try to get more packets to write while I `wait.'
            if (mPacketsQueue.empty())
            {
                try
                {
                    auto packetsBuffer
                         = mSubscriptionManager->getPackets(mContextAddress);
                    for (auto &packet : packetsBuffer)
                    {
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

            // No new packets were acquired and I'm not waiting for a write.
            // Give me stream manager a break.
            if (mPacketsQueue.empty() && !mWriteInProgress)
            {
                std::this_thread::sleep_for(mTimeOut);
            }
        }
        if (mContext)
        {
            // The context is still valid so try to remove it from the
            // subscriptions.  This can be the case whether the server is
            // shutting down or the client bailed.
            if (mSubscribed)
            {
                mSubscriptionManager->unsubscribeFromAll(mContextAddress);
                mSubscribed = false;
            }
            if (mContext->IsCancelled())
            {
                SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of client side cancel",
                    mPeer);
                Finish(grpc::Status::CANCELLED);
            }
            else
            {
                SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of server side cancel",
                    mPeer);
                Finish(grpc::Status::OK);
            }
        }
        else
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "The context for {} has disappeared",
                               mPeer);
        }
    }
    grpc::CallbackServerContext *mContext{nullptr};
    uintptr_t mContextAddress;
    SubscriberOptions mOptions;
    std::shared_ptr
    <
        UDataPacketService::SubscriptionManager
    > mSubscriptionManager{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> *mKeepRunning{nullptr};
    std::string mPeer;
    size_t mMaximumQueueSize{2048};
    std::queue<UDataPacketServiceAPI::V1::Packet> mPacketsQueue;
    std::chrono::milliseconds mTimeOut{20};
    bool mSubscribed{false};
    bool mWriteInProgress{false};
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
        const UDataPacketServiceAPI::V1::SubscriptionRequest *request,
        const SubscriberOptions &subscriberOptions,
        const bool isSecure,
        std::shared_ptr
        <
           UDataPacketService::SubscriptionManager
        > subscriptionManager,
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :     
        mContext(context),
        mContextAddress(reinterpret_cast<uintptr_t> (mContext)),
        mOptions(subscriberOptions),
        mSubscriptionManager(subscriptionManager),
        mLogger(logger),
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
        if (isSecure &&
            mOptions.getGRPCServerOptions().getAccessToken() != std::nullopt)
        {
            auto accessToken
                = *mOptions.getGRPCServerOptions().getAccessToken();
            if (!validateSubscriber(mContext, accessToken))
            {
                SPDLOG_LOGGER_INFO(mLogger, "Rejected {}", mPeer);
                grpc::Status status{grpc::StatusCode::UNAUTHENTICATED,
R"""(
Subscriber must provide access token in x-custom-auth-token header field.
)"""};
                Finish(status);
            }
            else
            {
                SPDLOG_LOGGER_INFO(mLogger, "Validated {}", mPeer);
            }
        }
        else
        {
            SPDLOG_LOGGER_INFO(mLogger, "{} connected to subscribe to all RPC", mPeer);
        }

        // Resource exhausted?
int maximumNumberOfSubscribers = 8;
        if (mSubscriptionManager->getNumberOfSubscribers() >=
            maximumNumberOfSubscribers)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                "Subscribe to all RPC rejecting {} because max number of subscribers hit",
                 mPeer);
            grpc::Status status{grpc::StatusCode::RESOURCE_EXHAUSTED,
                                "Max subscribers hit - try again later"};
            Finish(status);
       }

        // Subscribe
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
//            mMetrics.updateSubscriberUtilization(utilization);
            SPDLOG_LOGGER_INFO(mLogger,
                          "Now managing {} subscribers (Resource {} pct utilized)",
                          nSubscribers, utilization*100.0);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} failed to subscribe because {}",
                               mPeer, std::string {e.what()});
            Finish(grpc::Status(grpc::StatusCode::INTERNAL,
                                "Failed to subscribe"));
        }
        // Start
        nextWrite();

    }   

    void OnCancel() override
    {   
        SPDLOG_LOGGER_INFO(mLogger,
                           "Subscribe to all RPC cancelled for {}.",
                           mPeer);
        if (mSubscribed)
        {   
            mSubscriptionManager->unsubscribeFromAll(mContextAddress);
            mSubscribed = false;
        }   
    }   

#ifndef NDEBUG
    ~SubscribeToAll()
    {   
        SPDLOG_LOGGER_INFO(mLogger, "In destructor");
    }   
#endif

//private:
    void nextWrite()
    {
        // Keep running either until the server or client quits
        while (mKeepRunning->load())
        {
            if (mContext->IsCancelled()){break;}

            if (!mPacketsQueue.empty() && !mWriteInProgress)
            {
                const auto &packet = mPacketsQueue.front();
                mWriteInProgress = true;
                StartWrite(&packet);
                return;
            }

            // Try to get more packets to write while I `wait.'
            if (mPacketsQueue.empty())
            {
                try
                {
                    auto packetsBuffer
                         = mSubscriptionManager->getPackets(mContextAddress);
                    for (auto &packet : packetsBuffer)
                    {
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

            // No new packets were acquired and I'm not waiting for a write.
            // Give me stream manager a break.
            if (mPacketsQueue.empty() && !mWriteInProgress)
            {
                std::this_thread::sleep_for(mTimeOut);
            }
        }

        if (mContext)
        {
            if (mSubscribed)
            {
                mSubscriptionManager->unsubscribeFromAll(mContextAddress);
                mSubscribed = false;
            }
            if (mContext->IsCancelled())
            {
                SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of client side cancel",
                    mPeer);
                Finish(grpc::Status::CANCELLED);
            }
            else
            {
                SPDLOG_LOGGER_INFO(mLogger,
                 "Terminating acquisition for {} because of server side cancel",
                    mPeer);
                Finish(grpc::Status::OK);
            }
        }
        else
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "The context for {} has disappeared",
                               mPeer);
        }
    }

    grpc::CallbackServerContext *mContext{nullptr};
    uintptr_t mContextAddress;
    SubscriberOptions mOptions;
    std::shared_ptr
    <   
        UDataPacketService::SubscriptionManager
    > mSubscriptionManager{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> *mKeepRunning{nullptr};
    std::string mPeer;
    size_t mMaximumQueueSize{2048};
    std::queue<UDataPacketServiceAPI::V1::Packet> mPacketsQueue;
    std::chrono::milliseconds mTimeOut{20};
    bool mSubscribed{false};
    bool mWriteInProgress{false};
};

}
