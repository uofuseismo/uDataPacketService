#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <random>
#include <chrono>
#include <bit>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/grpcpp.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketService/server.hpp"
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"
#include "utilities.hpp"
#include "certs.hpp"

import Metrics;

#define GRPC_CLIENT_HOST "localhost"
#define GRPC_SERVER_HOST "0.0.0.0"
#define GRPC_PORT 48482

#define NETWORK "UU"
#define STATION "RDMU"
#define LOCATION_CODE "01"

using namespace UDataPacketService;

std::vector<UDataPacketServiceAPI::V1::Packet> referencePackets;
std::vector<UDataPacketServiceAPI::V1::Packet> subReferencePackets;

TEST_CASE("UDataPacketServer", "[ServerOptions]")
{
    const std::string host{"this-compy"};
    const uint16_t port{8424};
    UDataPacketService::GRPCServerOptions grpcOptions;
    grpcOptions.setHost(host);
    grpcOptions.setPort(port);

    ServerOptions options;
    constexpr int maxSubscribers{13};
    options.setGRPCOptions(grpcOptions);
    options.setMaximumNumberOfSubscribers(maxSubscribers);
    REQUIRE(options.getMaximumNumberOfSubscribers() == maxSubscribers);
    REQUIRE(options.getGRPCOptions().getHost() == host);
    REQUIRE(options.getGRPCOptions().getPort() == port);
}

///--------------------------------------------------------------------------///

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
    createChannel(const bool useCerts = false,
                  const bool useAPIKey = false,
                  const bool useClientCerts = false)
{
    auto address = std::string {GRPC_CLIENT_HOST}
                 + ":" + std::to_string(GRPC_PORT);
    if (useCerts || useClientCerts)
    {   
#ifndef NDEBUG
        if (useCerts){assert(!serverCertificate.empty());}
        if (useClientCerts){assert(!clientCertificate.empty());}
        if (useClientCerts){assert(!clientPrivateKey.empty());}
#endif
        if (useAPIKey)
        {
            auto callCredentials = grpc::MetadataCredentialsFromPlugin(
                std::unique_ptr<grpc::MetadataCredentialsPlugin> (
                    new ::CustomAuthenticator(apiKey)));
            grpc::SslCredentialsOptions sslOptions;
            sslOptions.pem_root_certs = serverCertificate;
            if (useClientCerts)
            {
                sslOptions.pem_cert_chain = clientCertificate; 
                sslOptions.pem_private_key = clientPrivateKey;
            }
            auto channelCredentials
                = grpc::CompositeChannelCredentials(
                      grpc::SslCredentials(sslOptions),
                      callCredentials);
            return grpc::CreateChannel(address, channelCredentials);
        }
        grpc::SslCredentialsOptions sslOptions;
        if (useCerts)
        {
            sslOptions.pem_cert_chain = clientCertificate;
            sslOptions.pem_root_certs = serverCertificate;
        } 
        if (useClientCerts)
        {
            sslOptions.pem_private_key = clientPrivateKey;
        }
        return grpc::CreateChannel(address,
                                   grpc::SslCredentials(sslOptions));
    } 
    return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

class Subscriber final :
    public grpc::ClientReadReactor<UDataPacketServiceAPI::V1::Packet>
{
public:
    Subscriber(UDataPacketServiceAPI::V1::Broadcast::Stub *stub,
               UDataPacketServiceAPI::V1::SubscribeToAllRequest &subscriptionRequest,
               std::vector<UDataPacketServiceAPI::V1::Packet> *receivedPackets) :
        mSubscribeToAllRequest(subscriptionRequest),
        mReceivedPackets(receivedPackets)
    {
        mContext.set_wait_for_ready(true);
        stub->async()->SubscribeToAll(&mContext, &mSubscribeToAllRequest, this);
        StartRead(&mPacket);
        StartCall();
    }   
    Subscriber(UDataPacketServiceAPI::V1::Broadcast::Stub *stub,
               UDataPacketServiceAPI::V1::SubscriptionRequest &subscriptionRequest,
               std::vector<UDataPacketServiceAPI::V1::Packet> *receivedPackets) :
        mSubscribeToSomeRequest(subscriptionRequest),
        mReceivedPackets(receivedPackets)
    {
        mContext.set_wait_for_ready(true);
        stub->async()->Subscribe(&mContext, &mSubscribeToSomeRequest, this);
        StartRead(&mPacket);
        StartCall();
    }

    void OnReadDone(bool ok) override
    {   
        if (ok)
        {   
#ifndef NDEBUG
            assert(mReceivedPackets);
#endif
            //std::cout << "yar" << std::endl;
            mReceivedPackets->push_back(mPacket);
            StartRead(&mPacket);
        }
    }   
    void OnDone(const grpc::Status &status) override
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mStatus = status;
        mDone = true;
        mConditionVariable.notify_one();
    }
    [[nodiscard]] grpc::Status await()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mConditionVariable.wait(lock, [this] {return mDone;});
        return std::move(mStatus);
    }
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    grpc::ClientContext mContext;
    UDataPacketServiceAPI::V1::SubscriptionRequest mSubscribeToSomeRequest;
    UDataPacketServiceAPI::V1::SubscribeToAllRequest mSubscribeToAllRequest;
    UDataPacketServiceAPI::V1::Packet mPacket;
    grpc::Status mStatus;
    std::vector<UDataPacketServiceAPI::V1::Packet> *mReceivedPackets{nullptr};
    bool mDone{false};
};



void subscribeToAll(const bool useCerts = false,
                    const bool useAPIKey = false)
{
    UDataPacketServiceAPI::V1::SubscribeToAllRequest request;
    request.set_identifier("asyncSubToAll");
    auto channel = createChannel(useCerts, useAPIKey); 
    auto stub = UDataPacketServiceAPI::V1::Broadcast::NewStub(channel);
    std::vector<UDataPacketServiceAPI::V1::Packet> receivedPackets;
    Subscriber subscriber{stub.get(), request, &receivedPackets};
    auto status = subscriber.await();
    REQUIRE(status.ok());
    REQUIRE(::comparePackets(receivedPackets, referencePackets));
}

void subscribeToSome(const bool useCerts = false,
                     const bool useAPIKey = false)
{
    UDataPacketServiceAPI::V1::SubscriptionRequest request;
    request.set_identifier("asyncSubTosome");
    const std::string network{NETWORK};
    const std::string station{STATION};
    const std::array<std::string, 3> channels{"HHN", "HHE"};
    const std::string locationCode{LOCATION_CODE};
    for (const auto &channel : channels)
    {
        auto selection = toIdentifier(network, station, channel, locationCode);
        *request.add_selections() = std::move(selection);
    }
 
    auto channel = createChannel(useCerts, useAPIKey); 
    auto stub = UDataPacketServiceAPI::V1::Broadcast::NewStub(channel);
    std::vector<UDataPacketServiceAPI::V1::Packet> receivedPackets;
    Subscriber subscriber{stub.get(), request, &receivedPackets};
    auto status = subscriber.await();
    REQUIRE(status.ok());
    REQUIRE(::comparePackets(receivedPackets, subReferencePackets));
}

std::vector<UDataPacketServiceAPI::V1::Packet> generate3CPackets(
    const int nPacketsPerChannel = 5)
{
    const std::string network{NETWORK};
    const std::string station{STATION};
    const std::array<std::string, 3> channels{"HHZ", "HHN", "HHE"};
    const std::string locationCode{LOCATION_CODE};
    auto p1 = ::generatePackets(nPacketsPerChannel, network,
                                station, channels.at(0), locationCode);
    auto p2 = ::generatePackets(nPacketsPerChannel, network,
                                station, channels.at(1), locationCode);
    auto p3 = ::generatePackets(nPacketsPerChannel, network,
                                station, channels.at(2), locationCode);
    // Intertwine
    std::vector<UDataPacketServiceAPI::V1::Packet> packets;
    for (int i = 0; i < nPacketsPerChannel; ++i)
    {
        packets.push_back(p1.at(i));
        packets.push_back(p2.at(i));
        packets.push_back(p3.at(i));
    }
    return packets;
}


TEST_CASE("UDataPacketServer", "[SubscribeToAll]")
{
    UDataPacketService::Metrics::initializeMetricsSingleton();

    bool useCerts{false};
    bool useAPIKey{false};
    bool useClientCert{false};
    GRPCServerOptions grpcServerOptions;
    grpcServerOptions.setHost(GRPC_SERVER_HOST);
    grpcServerOptions.setPort(GRPC_PORT);
    if (useCerts)
    {
        grpcServerOptions.setServerCertificate(serverCertificate);
        grpcServerOptions.setServerKey(serverPrivateKey);
        if (useAPIKey){grpcServerOptions.setAccessToken(apiKey);}
    }
    if (useClientCert)
    {
        grpcServerOptions.setClientCertificate(clientCertificate);
    }

    ServerOptions serverOptions;
    serverOptions.setMaximumNumberOfSubscribers(16);
    serverOptions.setGRPCOptions(grpcServerOptions);

    referencePackets = ::generate3CPackets();
    for (const auto &p : referencePackets)
    {
        if (p.stream_identifier().channel() != "HHZ")
        {
            subReferencePackets.push_back(p);
        }
    }
    

    auto logger = spdlog::stdout_color_mt("console");
    auto server = std::make_unique<Server> (serverOptions, logger);
    auto serverThread = std::thread(&Server::start, &*server);
    auto clientSubAllThread = std::thread(&subscribeToAll,
                                          useCerts, useAPIKey);
    auto clientSubSomeThread = std::thread(&subscribeToSome,
                                           useCerts, useAPIKey);

    std::this_thread::sleep_for(std::chrono::milliseconds {1000});
    for (const auto &packet : referencePackets)
    {
        server->enqueuePacket(packet);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds {1000});
    server->stop();
    if (serverThread.joinable()){serverThread.join();}
    if (clientSubAllThread.joinable()){clientSubAllThread.join();}
    if (clientSubSomeThread.joinable()){clientSubSomeThread.join();}
}


