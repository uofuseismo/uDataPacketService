#include <mutex>
#include <atomic>
#include <condition_variable>
#include <spdlog/spdlog.h>
#include <grpc/grpc.h>
#include "uDataPacketService/subscriberOptions.hpp"
#include "uDataPacketImportAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/backend.grpc.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

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
        std::function<void (const UDataPacketImportAPI::V1::Packet &)> &addPacketCallback,
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
                mAddPacketCallback(mPacket);
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
        void (const UDataPacketImportAPI::V1::Packet &packet)
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
