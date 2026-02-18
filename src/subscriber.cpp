#include <mutex>
#include <atomic>
#include <condition_variable>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include "uDataPacketService/subscriber.hpp"
#include "uDataPacketService/grpcOptions.hpp"
#include "uDataPacketService/subscriberOptions.hpp"
#include "uDataPacketImportAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/backend.grpc.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

using namespace UDataPacketService;

namespace
{

class CustomAuthenticator : public grpc::MetadataCredentialsPlugin
{    
public:    
    CustomAuthenticator(const grpc::string &token) :
        mToken(token)
    {   
    }   
    grpc::Status GetMetadata(
        grpc::string_ref, // serviceURL, 
        grpc::string_ref, // methodName,
        const grpc::AuthContext &,//channelAuthContext,
        std::multimap<grpc::string, grpc::string> *metadata) override
    {   
        metadata->insert(std::make_pair("x-custom-auth-token", mToken));
        return grpc::Status::OK;
    }   
//private:
    grpc::string mToken;
};

std::shared_ptr<grpc::Channel>
    createChannel(const UDataPacketService::GRPCOptions &options,
                  spdlog::logger *logger)
{
    auto address = UDataPacketService::makeAddress(options);
    auto serverCertificate = options.getServerCertificate();
    if (serverCertificate)
    {
#ifndef NDEBUG
        assert(!serverCertificate->empty());
#endif
        if (options.getAccessToken())
        {
            auto apiKey = *options.getAccessToken();
#ifndef NDEBUG
            assert(!apiKey.empty());
#endif
            SPDLOG_LOGGER_INFO(logger,
                               "Creating secure channel with API key to {}",
                               address);
            auto callCredentials = grpc::MetadataCredentialsFromPlugin(
                std::unique_ptr<grpc::MetadataCredentialsPlugin> (
                    new ::CustomAuthenticator(apiKey)));
            grpc::SslCredentialsOptions sslOptions;
            sslOptions.pem_root_certs = *serverCertificate;
            auto channelCredentials
                = grpc::CompositeChannelCredentials(
                      grpc::SslCredentials(sslOptions),
                      callCredentials);
            return grpc::CreateChannel(address, channelCredentials);
        }
        SPDLOG_LOGGER_INFO(logger,
                           "Creating secure channel without API key to {}",
                           address);
        grpc::SslCredentialsOptions sslOptions;
        sslOptions.pem_root_certs = *serverCertificate;
        return grpc::CreateChannel(address,
                                   grpc::SslCredentials(sslOptions));
     }
     SPDLOG_LOGGER_INFO(logger,
                        "Creating non-secure channel to {}",
                         address);
     return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

class AsyncPacketSubscriber :
    public grpc::ClientReadReactor<UDataPacketImportAPI::V1::Packet>
{
public:
    AsyncPacketSubscriber
    (
        UDataPacketImportAPI::V1::Backend::Stub *stub,
        const UDataPacketImportAPI::V1::SubscriptionRequest &request,
        std::function<void (UDataPacketImportAPI::V1::Packet &&)> &addPacketCallback,
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :
        mRequest(request),
        mAddPacketCallback(addPacketCallback),
        mLogger(logger),
        mKeepRunning(keepRunning)
    {
        mClientContext.set_wait_for_ready(false); // Fail immediately if server isn't there
        stub->async()->Subscribe(&mClientContext, &mRequest, this); 
        StartRead(&mPacket);
        StartCall();
    }

    void OnReadDone(bool ok) override
    {
        if (ok)
        {
            mHadSuccessfulRead = true;
            try
            {
                auto copy = mPacket;
                mAddPacketCallback(std::move(copy));
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_ERROR(mLogger,
                                    "Failed to add packet to callback because {}",
                                    std::string {e.what()});
            }
            if (!mKeepRunning->load())
            {
                //Finish(grpc::Status::OK);
                mClientContext.TryCancel();
            }
            StartRead(&mPacket);
        }
        else
        {
            if (!mKeepRunning->load())
            {
                //Finish(grpc::Status::OK);
                mClientContext.TryCancel();
            }
        } 
    }

    void OnDone(const grpc::Status &status) override
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mStatus = status; 
        mDone = true;
        mConditionVariable.notify_one();
    }

    [[nodiscard]] std::pair<grpc::Status, bool> await()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mConditionVariable.wait(lock, [this] {return mDone;});
        return std::pair{std::move(mStatus), mHadSuccessfulRead};
    }

#ifndef NDEBUG
    ~AsyncPacketSubscriber()
    {
        SPDLOG_LOGGER_DEBUG(mLogger, "In destructor");
    }
#endif
private:
    grpc::ClientContext mClientContext;
    UDataPacketImportAPI::V1::SubscriptionRequest mRequest;
    std::function
    <
        void (UDataPacketImportAPI::V1::Packet &&packet)
    > mAddPacketCallback;
    std::shared_ptr<spdlog::logger> mLogger;
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    UDataPacketImportAPI::V1::Packet mPacket;
    grpc::Status mStatus{grpc::Status::OK};
    bool mDone{false};
    std::atomic<bool> *mKeepRunning{nullptr};
    bool mHadSuccessfulRead{false}; 
};

}

class Subscriber::SubscriberImpl
{
public:
    SubscriberImpl(const SubscriberOptions &options,
                   const std::function<void (UDataPacketImportAPI::V1::Packet &&)> &callback,
                   std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mAddPacketCallback(callback),
        mLogger(logger)
    {
    }
                   
    ~SubscriberImpl()
    {
        stop();
    }
    void stop()
    {
        mShutdownRequested = true;
        mShutdownCondition.notify_all();
        mKeepRunning.store(false);
    }
    [[nodiscard]] std::future<void> start()
    {
        mShutdownRequested = false;
        mKeepRunning.store(true);
        auto result = std::async(&SubscriberImpl::acquirePackets, this);
        return result;
    }
    void acquirePackets()
    {
#ifndef NDEBUG
        assert(mLogger != nullptr);
#endif
        auto reconnectSchedule = mOptions.getReconnectSchedule();
        auto nReconnect = static_cast<int> (reconnectSchedule.size());
        for (int kReconnect =-1; kReconnect < nReconnect; ++kReconnect)
        {
            if (!mKeepRunning.load()){break;}
            if (kReconnect >= 0)
            {
                SPDLOG_LOGGER_INFO(mLogger,
                                   "Will attempt to reconnect in {} s",
                                   reconnectSchedule.at(kReconnect).count());
                std::unique_lock<std::mutex> lock(mShutdownMutex);
                mShutdownCondition.wait_for(lock,
                                            reconnectSchedule.at(kReconnect),
                                            [this]
                                            {
                                                return mShutdownRequested;
                                            });
                lock.unlock();
                if (!mKeepRunning.load()){break;}
            }
            // Create channel
            auto channel
                = ::createChannel(mOptions.getGRPCOptions(), mLogger.get());
            auto stub = UDataPacketImportAPI::V1::Backend::NewStub(channel);
            UDataPacketImportAPI::V1::SubscriptionRequest request;
            auto subscriberIdentifier = mOptions.getIdentifier();
            if (subscriberIdentifier)
            {
                request.set_identifier(*subscriberIdentifier);
            }
            AsyncPacketSubscriber subscriber{stub.get(),
                                             request,
                                             mAddPacketCallback,
                                             mLogger,
                                             &mKeepRunning};
            auto [status, hadSuccessfulRead] = subscriber.await();
            if (hadSuccessfulRead){kReconnect =-1;}
            if (status.ok())
            {
                if (!mKeepRunning.load())
                {
                    SPDLOG_LOGGER_INFO(mLogger,
                                       "Subscriber RPC successfully finished");
                    break;
                }
                else
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Subscriber RPC successfully finished but I should keep reading");
                }
            }
            else
            {
                int errorCode(status.error_code());
                std::string errorMessage(status.error_message());
                if (errorCode == grpc::StatusCode::UNAVAILABLE)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Server unavailable (message: {})",
                                       errorMessage);
                }
                else if (errorCode == grpc::StatusCode::CANCELLED)
                {
                    if (!mKeepRunning.load()){break;}
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Server-side cancel (message: {})",
                                       errorMessage);
                }
                else
                {
                    SPDLOG_LOGGER_ERROR(mLogger,
                             "Subscribe RPC failed with error code {} (what: {})",
                             errorCode,  errorMessage);
                    break;
                }
            }
        } // Loop on retries
        if (mKeepRunning.load())
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Subscriber thread quitting!");
            throw std::runtime_error("Premature end of subscriber thread");
        }
        SPDLOG_LOGGER_INFO(mLogger, "Subscriber thread exiting");
    }
//private:
    SubscriberOptions mOptions;
    std::function
    <   
        void (UDataPacketImportAPI::V1::Packet &&packet)
    > mAddPacketCallback;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    mutable std::mutex mShutdownMutex;
    std::condition_variable mShutdownCondition;
    std::atomic<bool> mKeepRunning{true};
    bool mShutdownRequested{false};
};


/// Constructor
Subscriber::Subscriber
(
    const SubscriberOptions &options,
    const std::function<void (UDataPacketImportAPI::V1::Packet &&)> &callback,
    std::shared_ptr<spdlog::logger> logger
) :
    pImpl(std::make_unique<SubscriberImpl> (options, callback, logger))
{
}

/// Start the subscriber
std::future<void> Subscriber::start()
{
    return pImpl->start();
}

/// Stop the subscriber
void Subscriber::stop()
{
    pImpl->stop();
}

/// Destructor
Subscriber::~Subscriber() = default;
