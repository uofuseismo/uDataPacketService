module;
#include <atomic>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"

export module Server;
import AsyncWriter;

namespace UDataPacketService
{
 
export
class Service : public UDataPacketServiceAPI::V1::Broadcast::CallbackService
{
public:
    Service(const ServerOptions &options,
            std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger)
    {
        if (mLogger == nullptr){throw std::invalid_argument("Logger is null");}
        mSubscriptionManager
            = std::make_unique<UDataPacketService::SubscriptionManager>
              (mOptions.getSubscriptionManagerOptions(), mLogger);
        if (mSubscriptionManager == nullptr)
        {
            throw std::runtime_error("Failed to create subscription manager");
        }
    }    

    ~Service() override
    {   
        stop();
        std::this_thread::sleep_for(std::chrono::milliseconds {15});
        if (mServer)
        {
            mServer->Shutdown();
        }
    }   

    void start()
    {
        mKeepRunning.store(true);
        auto grpcOptions = mOptions.getGRPCOptions();
        auto address = makeAddress(grpcOptions);
        grpc::ServerBuilder builder;
        if (grpcOptions.getServerKey() == std::nullopt ||
            grpcOptions.getServerCertificate() == std::nullopt)
        {
            SPDLOG_LOGGER_INFO(mLogger, "Initiating non-secured service");
            builder.AddListeningPort(address,
                                     grpc::InsecureServerCredentials());
            builder.RegisterService(this);
            mSecureConnection = false;
        }
        else
        {
            SPDLOG_LOGGER_INFO(mLogger, "Initiating secured service");
            grpc::SslServerCredentialsOptions::PemKeyCertPair keyCertPair
            {   
                *grpcOptions.getServerKey(),        // Private key
                *grpcOptions.getServerCertificate() // Public key (cert chain)
            };  
            grpc::SslServerCredentialsOptions sslOptions; 
            sslOptions.pem_key_cert_pairs.emplace_back(keyCertPair);
            builder.AddListeningPort(address,
                                     grpc::SslServerCredentials(sslOptions));
            builder.RegisterService(this);
            mSecureConnection = true;
        }   

        SPDLOG_LOGGER_INFO(mLogger, "Server listening at {}", address);
        mServer = builder.BuildAndStart();
    }

    void stop()
    {   
        mKeepRunning.store(false);
        // Give RPCs a chance to give up
        std::this_thread::sleep_for(std::chrono::milliseconds {15});
        mSubscriptionManager->unsubscribeAll();
    }

    /// Subscribes to specific streams
    grpc::ServerWriteReactor<UDataPacketServiceAPI::V1::Packet> *
        Subscribe(grpc::CallbackServerContext* context,
                  const UDataPacketServiceAPI::V1::SubscriptionRequest *request) override
    {   
        return new
            UDataPacketService::Subscribe(context,
                                          request,
                                          mOptions,
                                          mSecureConnection,
                                          mSubscriptionManager,
                                          mLogger,
                                          &mKeepRunning);
    }

    /// Subscribes to all streams
    grpc::ServerWriteReactor<UDataPacketServiceAPI::V1::Packet> *
        SubscribeToAll(grpc::CallbackServerContext* context,
                       const UDataPacketServiceAPI::V1::SubscribeToAllRequest *request) override
    {   
        return new 
            UDataPacketService::SubscribeToAll(context,
                                               request,
                                               mOptions,
                                               mSecureConnection,
                                               mSubscriptionManager,
                                               mLogger,
                                               &mKeepRunning);
    }



    /// Allows producers to add packets to subscription manager
    void enqueuePacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        mSubscriptionManager->enqueuePacket(std::move(packet));
    }

    [[nodiscard]] int getNumberOfSubscribers() const noexcept
    {
        return mSubscriptionManager->getNumberOfSubscribers();
    }
private:
    ServerOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::shared_ptr<SubscriptionManager> mSubscriptionManager{nullptr};
    std::unique_ptr<grpc::Server> mServer{nullptr};
    std::atomic<bool> mKeepRunning{true};
    bool mSecureConnection{false};  

};

}
