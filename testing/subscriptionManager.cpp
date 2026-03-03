#include <cmath>
#include <string>
#include <thread>
#include <random>
#include <chrono>
#include <bit>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <google/protobuf/util/time_util.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "utilities.hpp"

TEST_CASE("UDataPacketService", "[SubscriptionManagerOptions]")
{
    using namespace UDataPacketService;
    //constexpr int maxSubscribers{599};
    constexpr int maxStreamQueueSize{832};
    StreamOptions streamOptions;
    streamOptions.setMaximumQueueSize(maxStreamQueueSize);

    SECTION("Set/get")
    {
        SubscriptionManagerOptions options;
        //options.setMaximumNumberOfSubscribers(maxSubscribers);
        options.setStreamOptions(streamOptions);
        //REQUIRE(options.getMaximumNumberOfSubscribers() == maxSubscribers);
        REQUIRE(options.getStreamOptions().getMaximumQueueSize() == maxStreamQueueSize);
    }

    SECTION("Defaults")
    {
        SubscriptionManagerOptions options;
        REQUIRE(options.getStreamOptions().getMaximumQueueSize() == 8);
    }
}

TEST_CASE("UDataPacketServer", "[SubscriptionManager]")
{
    const std::array<std::string, 3> channels{"HHZ", "HHN", "HHE"};
    const std::string network{"UU"};
    const std::string station{"CWU"};
    const std::string locationCode{"01"};

    using namespace UDataPacketService;
    SubscriptionManagerOptions defaultOptions;

    SECTION("SubscribeToAll")
    {
        auto consoleSink
            = std::make_shared<spdlog::sinks::stdout_color_sink_mt> (); 
        auto logger
            = std::make_shared<spdlog::logger>
              (spdlog::logger ("SubscriptionManagerTestSubscribeToAll",
               {consoleSink}));

        SubscriptionManager subscriptionManager{defaultOptions, logger};

        // Create the subscribers
        auto identifier1 = ::toIdentifier(network, station, channels.at(0), locationCode);
        auto identifier2 = ::toIdentifier(network, station, channels.at(1), locationCode);
        auto identifier3 = ::toIdentifier(network, station, channels.at(2), locationCode);

        auto myThreadID = std::this_thread::get_id();
        auto subscriberID1 = reinterpret_cast<uintptr_t> (&myThreadID);
        auto subscriberID2 = reinterpret_cast<uintptr_t> (&myThreadID) + 1;
 
        REQUIRE(subscriptionManager.getNumberOfSubscribers() == 0);

        // First thread subscribes to all
        subscriptionManager.subscribeToAll(subscriberID1);
        REQUIRE(subscriptionManager.getNumberOfSubscribers() == 1);

        // Second thread subscribes to some
        std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streamIdentifiers;
        streamIdentifiers.push_back(identifier2);
        streamIdentifiers.push_back(identifier3);
        streamIdentifiers.push_back(identifier3); // Duplicate
        subscriptionManager.subscribe(subscriberID2, streamIdentifiers);
        REQUIRE(subscriptionManager.getNumberOfSubscribers() == 2);
        
       

        // Create a publisher
        constexpr int nPacketsPerChannel{5};
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
        
        int iPacket{0};
        for (int k = 0; k < static_cast<int> (packets.size()); ++k)
        {
            std::vector<UDataPacketServiceAPI::V1::Packet> sentPackets;
            std::vector<UDataPacketServiceAPI::V1::Packet> selectedPackets;
            for (int i = 0; i < 3; ++i)
            {
                if (iPacket >= static_cast<int> (packets.size())){break;}
                subscriptionManager.enqueuePacket(packets.at(iPacket)); 
                sentPackets.push_back(packets.at(iPacket));
                if (i > 0)
                {
                    selectedPackets.push_back(packets[iPacket]);
                }
                iPacket = iPacket + 1;
            }
            auto nextPackets = subscriptionManager.getPackets(subscriberID1);
            REQUIRE(nextPackets.size() == sentPackets.size());
            //std::cout << nSent << " " << nextPacket.size() << std::endl;
            for (int i = 0; i < static_cast<int> (sentPackets.size()); ++i)
            {
                REQUIRE(sentPackets.at(i).start_time() ==
                        nextPackets.at(i).start_time());
                bool foundIt{false};
                for (int j = 0; j < static_cast<int> (nextPackets.size()); ++j)
                {
                    if (sentPackets.at(i).stream_identifier().channel() ==
                        nextPackets.at(j).stream_identifier().channel())
                    {
                        foundIt = true;
                        break;
                    }
                }
                REQUIRE(foundIt);
            }

            nextPackets = subscriptionManager.getPackets(subscriberID2);
            REQUIRE(nextPackets.size() == selectedPackets.size());
            for (int i = 0; i < static_cast<int> (selectedPackets.size()); ++i)
            {
                REQUIRE(selectedPackets.at(i).start_time() ==
                        nextPackets.at(i).start_time());
                bool foundIt{false};
                for (int j = 0; j < static_cast<int> (nextPackets.size()); ++j)
                {
                    if (selectedPackets.at(i).stream_identifier().channel() ==
                        nextPackets.at(j).stream_identifier().channel())
                    {
                        foundIt = true;
                        break;
                    }
                }
                REQUIRE(foundIt);
            }
        }

        subscriptionManager.unsubscribeFromAll(subscriberID2);
        REQUIRE(subscriptionManager.getNumberOfSubscribers() == 1);
        subscriptionManager.unsubscribeFromAll(subscriberID1);
        REQUIRE(subscriptionManager.getNumberOfSubscribers() == 0);
    }


}
