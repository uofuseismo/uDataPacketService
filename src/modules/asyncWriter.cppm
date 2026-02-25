module;
#include <string>
#include <atomic>
#ifndef NDEBUG
#include <cassert>
#endif
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"


export module AsyncWriter;

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
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :
        mContext(context),
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
        }
        if (mContext)
        {
/*
// TODO uncomment
            // The context is still valid so try to remove it from the
            // subscriptoins.  This can be the case whether the server is
            // shutting down or the client bailed.
            mSubscriptionManager->unsubscribe(mContext);
            mSubscribed = false;
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
*/
        }
        else
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "The context for {} has disappeared", mPeer);
        }
    }
    grpc::CallbackServerContext *mContext{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> *mKeepRunning{nullptr};
    std::string mPeer;
    bool mSubscribed{false};
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
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :
        mContext(context),
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
            // Cancel means we leave now
            if (mContext->IsCancelled()){break;}
        }
        if (mContext)
        {
/*
// TODO uncomment
            // The context is still valid so try to remove it from the
            // subscriptoins.  This can be the case whether the server is
            // shutting down or the client bailed.
            mSubscriptionManager->unsubscribe(mContext);
            mSubscribed = false;
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
*/
        }
        else
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "The context for {} has disappeared", mPeer);
        }
    }

    grpc::CallbackServerContext *mContext{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> *mKeepRunning{nullptr};
    std::string mPeer;
    bool mSubscribed{false};
};

}
