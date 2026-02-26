#include <cmath>
#include <string>
#include <thread>
#include <random>
#include <chrono>
#include <bit>
#include <google/protobuf/util/time_util.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketService/grpcOptions.hpp"
#include "utilities.hpp"

namespace
{

[[nodiscard]] std::chrono::microseconds getNowSimple() 
{
     auto now 
        = std::chrono::duration_cast<std::chrono::seconds>
          ((std::chrono::high_resolution_clock::now()).time_since_epoch());
     return now;
}

std::vector<UDataPacketServiceAPI::V1::Packet>
    generatePackets(int nPackets = 5,
                    const std::string &network = "UU",
                    const std::string &station = "CWU",
                    const std::string &channel = "HHZ",
                    const std::string &locationCode = "01")
{
    namespace UV1 = UDataPacketServiceAPI::V1;
    std::vector<UDataPacketServiceAPI::V1::Packet> result;
    constexpr double samplingRate{100};
    UV1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    UV1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    packet.set_sampling_rate(samplingRate);

    auto startTimeMuS
        = ::getNowSimple()
        - std::chrono::seconds {nPackets*(300/100) + 1};
    std::mt19937 generator(23883823);
    std::uniform_int_distribution<int> distribution(200, 300);
    int sample{0};
    for (int i = 0; i < nPackets; ++i)
    {
        auto nSamples = distribution(generator);
        std::vector<int> data(nSamples);
        for (int k = 0; k < nSamples; ++k)
        {
            data[k] = sample;
            sample++;
        }
        auto packedData = ::pack(data);

        startTimeMuS
            = startTimeMuS
            + std::chrono::microseconds
              {
              static_cast<int64_t> (std::round(1000000*nSamples/samplingRate))
              };
        auto startTime
             = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 startTimeMuS.count());

        UV1::Packet packet;
        *packet.mutable_stream_identifier() = identifier;
        *packet.mutable_start_time() = startTime;
        packet.set_sampling_rate(samplingRate);
        packet.set_number_of_samples(data.size());
        packet.set_data_type(UV1::DataType::DATA_TYPE_INTEGER_32);
        packet.set_data(packedData);

        result.push_back(std::move(packet));
    }
    return result;
}

}

TEST_CASE("UDataPacketService", "[streamOptions]")
{
    constexpr int maxQueueSize{5};
    using namespace UDataPacketService;
    StreamOptions options;
    options.setMaximumQueueSize(maxQueueSize);
    REQUIRE(options.getMaximumQueueSize() == maxQueueSize);
}

TEST_CASE("UDataPacketService", "[stream]")
{
    using namespace UDataPacketService;
    constexpr int nPacketsToCreate{5};
    const std::string network{"UU"};
    const std::string station{"CTU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    SECTION("Unordered")
    {
        StreamOptions options;
        auto inputPackets
            = ::generatePackets(nPacketsToCreate,
                                network,
                                station,
                                channel,
                                locationCode);
        std::mt19937 generator(939392);
        std::shuffle(inputPackets.begin(), inputPackets.end(), generator);

        REQUIRE(inputPackets.size() == nPacketsToCreate);

        std::vector<UDataPacketServiceAPI::V1::Packet> packetsBack1;
        std::vector<UDataPacketServiceAPI::V1::Packet> packetsBack2;

        auto packet = inputPackets.at(0);
        UDataPacketService::Stream stream{std::move(packet), options};
        REQUIRE(stream.getIdentifier() == "UU.CTU.HHZ.01");

        auto myThreadID = std::this_thread::get_id();
        auto subscriberID1 = reinterpret_cast<uintptr_t> (&myThreadID);
        auto subscriberID2 = subscriberID1 + 1; 
        constexpr bool enqueuePacket{true};
        REQUIRE(stream.subscribe(subscriberID1, enqueuePacket));

        {
        auto packetBack = stream.getNextPacket(subscriberID1);
        if (packetBack){packetsBack1.push_back(*packetBack);}
        }

        REQUIRE(stream.subscribe(subscriberID2, enqueuePacket));
        {
        auto packetBack = stream.getNextPacket(subscriberID2);
        if (packetBack){packetsBack2.push_back(*packetBack);}
        }

        REQUIRE(stream.getNumberOfSubscribers() == 2);
        REQUIRE(!stream.subscribe(subscriberID1, enqueuePacket));

        // Get everything back on the first thread 
        for (int i = 1; i < nPacketsToCreate; ++i)
        {
            stream.setNextPacket(inputPackets.at(i));
            auto packetBack = stream.getNextPacket(subscriberID1);
            if (packetBack){packetsBack1.push_back(*packetBack);}
        }

        // And the other thread
        for (int i = 1; i < nPacketsToCreate; ++i)
        {
            auto packetBack = stream.getNextPacket(subscriberID2);
            if (packetBack){packetsBack2.push_back(*packetBack);}
        }

        auto subscribers = stream.getSubscribers();
        REQUIRE(stream.isSubscribed(subscriberID1));
        REQUIRE(stream.isSubscribed(subscriberID2));
        REQUIRE(subscribers.size() == 2);
        REQUIRE(subscribers.contains(subscriberID1));
        REQUIRE(subscribers.contains(subscriberID2));

        REQUIRE(stream.unsubscribe(subscriberID1));
        REQUIRE(stream.getNumberOfSubscribers() == 1);
        REQUIRE(stream.unsubscribe(subscriberID2));
        REQUIRE(stream.getNumberOfSubscribers() == 0);

        REQUIRE(inputPackets.size() == packetsBack1.size());
        REQUIRE(inputPackets.size() == packetsBack2.size());
        for (int i = 0; i < static_cast<int> (inputPackets.size()); ++i)
        {
            REQUIRE(inputPackets.at(i).data_type() ==
                    packetsBack1.at(i).data_type());
            REQUIRE(inputPackets.at(i).start_time() ==
                    packetsBack1.at(i).start_time());
            REQUIRE(inputPackets.at(i).number_of_samples() ==
                    packetsBack1.at(i).number_of_samples());

            REQUIRE(inputPackets.at(i).data_type() ==
                    packetsBack2.at(i).data_type());
            REQUIRE(inputPackets.at(i).start_time() ==
                    packetsBack2.at(i).start_time());
            REQUIRE(inputPackets.at(i).number_of_samples() ==
                    packetsBack2.at(i).number_of_samples());
        }
    }
}

