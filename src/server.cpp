#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <spdlog/logger.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/time.h> //NOLINT
#include <spdlog/spdlog.h>
#include "uDataPacketService/server.hpp"
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"
#include "uDataPacketServiceAPI/v1/subscribe_to_all_request.pb.h"
#include "uDataPacketServiceAPI/v1/subscription_request.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

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
        mLogger(std::move(logger))
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
        auto serverKey = grpcOptions.getServerKey();
        auto serverCertificate = grpcOptions.getServerCertificate();
        if (serverKey == std::nullopt ||
            serverCertificate == std::nullopt)
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
            const grpc::SslServerCredentialsOptions::PemKeyCertPair keyCertPair
            {   
                *serverKey,        // Private key
                *serverCertificate // Public key (cert chain)
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
        mServerStarted.store(true);
        // If stop() ran before the server came up then shut down immediately
        if (!mKeepRunning.load()){mServer->Shutdown();}
        mServer->Wait();
        mServerStarted.store(false);
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
        // Signal the server to shut down.  Do not destroy mServer here:
        // the start() thread is still inside mServer->Wait() and Shutdown()
        // is what unblocks it.  The server is destroyed with this class,
        // after the start() future has been joined.
        if (mServer && mServerStarted.load())
        {
            SPDLOG_LOGGER_INFO(mLogger, "Shutting down service");
            const auto shutdownDeadline = mOptions.getShutdownDeadline();
            // tv_nsec is an int32 and must stay below 1e9, so split the
            // deadline into whole seconds and remainder nanoseconds
            const auto timeOutSeconds
                = std::chrono::duration_cast<std::chrono::seconds>
                  (shutdownDeadline);
            const auto timeOutNanoSeconds
                = std::chrono::duration_cast<std::chrono::nanoseconds>
                  (shutdownDeadline - timeOutSeconds);
            const gpr_timespec deadline // NOLINT
            {
                timeOutSeconds.count(),
                static_cast<int32_t> (timeOutNanoSeconds.count()),
                GPR_TIMESPAN // NOLINT
            };
            mServer->Shutdown(deadline);
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
    std::atomic<bool> mServerStarted{false};
    bool mSecureConnection{false};
};

/// Constructor
Server::Server(const ServerOptions &options,
               std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<ServerImpl> (options, std::move(logger)))
{
}

/// Start
std::future<void> Server::start()
{
    return std::async(&ServerImpl::start, &*pImpl);
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
