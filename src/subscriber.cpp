#include <mutex>
#include <atomic>
#include <condition_variable>
#include <spdlog/spdlog.h>
#include <grpc/grpc.h>
#include "uDataPacketService/subscriber.hpp"
#include "uDataPacketService/grpcOptions.hpp"
#include "uDataPacketService/subscriberOptions.hpp"
#include "uDataPacketImportAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/backend.grpc.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

using namespace UDataPacketService;

namespace
{

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
        stub->async()->Subscribe(&mClientContext, &mRequest, this); 
        StartRead(&mPacket);
        StartCall();
    }

    void OnReadDone(bool ok) override
    {
        if (ok)
        {
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
        mKeepRunning.store(false);
    }
    [[nodiscard]] std::future<void> start()
    {
        mKeepRunning.store(true);
        auto result = std::async(&SubscriberImpl::acquirePackets, this);
        return result;
    }
    void acquirePackets()
    {
        auto reconnectSchedule = mOptions.getReconnectSchedule();
        reconnectSchedule.insert(reconnectSchedule.begin(),
                                 std::chrono::milliseconds {0});
        for (int k = 0; k < static_cast<int> (reconnectSchedule.size()); ++k)
        {
        }         
    }
//private:
    SubscriberOptions mOptions;
    std::function
    <   
        void (UDataPacketImportAPI::V1::Packet &&packet)
    > mAddPacketCallback;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::atomic<bool> mKeepRunning{true};
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
