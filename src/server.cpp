#include <atomic>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "uDataPacketService/server.hpp"
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"

import AsyncWriter;

using namespace UDataPacketService;

class Server::ServerImpl :
    public UDataPacketServiceAPI::V1::Broadcast::CallbackService
{
public:
    /// Constructor
    ServerImpl(const ServerOptions &options,
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

    /// Destructor
    ~ServerImpl() override
    {   
        stop();
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
        mServerStarted = true;
    }

    /// Stop the service
    void stop()
    {   
        // RPCs should see this and issue shutdown
        mKeepRunning.store(false);
        std::this_thread::sleep_for(std::chrono::milliseconds {15});
        // Forceably purge the remaning subscriptions
        mSubscriptionManager->unsubscribeAll();
        std::this_thread::sleep_for(std::chrono::milliseconds {15});
        // Kill the server
        if (mServer && mServerStarted)
        {
            mServer->Shutdown();
            mServerStarted = false;
        }
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

    /// Number of packets.
    [[nodiscard]] int getNumberOfSubscribers() const noexcept
    {
        return mSubscriptionManager->getNumberOfSubscribers();
    }
//private:
    ServerOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::shared_ptr<SubscriptionManager> mSubscriptionManager{nullptr};
    std::unique_ptr<grpc::Server> mServer{nullptr};
    std::atomic<bool> mKeepRunning{true};
    bool mSecureConnection{false};  
    bool mServerStarted{false};
};

/// Constructor
Server::Server(const ServerOptions &options,
               std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<ServerImpl> (options, logger))
{
}

/// Start
void Server::start()
{
    pImpl->start();
}

/// Stop
void Server::stop()
{
    pImpl->stop();
}

/// Enqueue packet
void Server::enqueuePacket(UDataPacketServiceAPI::V1::Packet &&packet)
{
    pImpl->enqueuePacket(std::move(packet));
}

void Server::enqueuePacket(const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto copy = packet;
    pImpl->enqueuePacket(std::move(copy));
}

/// Number of subscribers
int Server::getNumberOfSubscribers() const noexcept
{
    return pImpl->getNumberOfSubscribers();
}

/// Destructor
Server::~Server() = default;
