#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <numeric>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketService/expiredPacketDetector.hpp"
#include "uDataPacketService/futurePacketDetector.hpp"
#include "uDataPacketService/duplicatePacketDetector.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "utilities.hpp"

import Utilities;

using namespace UDataPacketService;

TEST_CASE("UDataPacketService::FuturePacketDetector", "[futureDataOptions]")
{
    constexpr std::chrono::microseconds maxFutureTime{1000};
    FuturePacketDetectorOptions options;
    options.setMaxFutureTime(maxFutureTime);
    REQUIRE(options.getMaxFutureTime() == maxFutureTime);
}

TEST_CASE("UDataPacketService::ExpiredPacketDetector", "[expiredDataOptions]")
{       
    constexpr std::chrono::microseconds maxExpiredTime{10000};
    ExpiredPacketDetectorOptions options;
    options.setMaxExpiredTime(maxExpiredTime);
    REQUIRE(options.getMaxExpiredTime() == maxExpiredTime);
}

TEST_CASE("UDataPacketService::DuplicatePacketDetector", "[duplicateDataOptions]")
{
    SECTION("cb size")
    {
        constexpr int circularBufferSize{129};
        DuplicatePacketDetectorOptions options;
        options.setCircularBufferSize(circularBufferSize);
        REQUIRE(*options.getCircularBufferSize() == circularBufferSize);
    }
    SECTION("cb duration")
    {
        constexpr std::chrono::seconds duration{90};
        DuplicatePacketDetectorOptions options;
        options.setCircularBufferDuration(duration);
        REQUIRE(*options.getCircularBufferDuration() == duration); 
    }
}

TEST_CASE("UDataPacketService::FuturePacketDetector", "[futureData]")
{
    namespace UV1 = UDataPacketServiceAPI::V1;
    constexpr auto dataType{UV1::DataType::DATA_TYPE_INTEGER_32};
    UV1::StreamIdentifier identifier;
    identifier.set_network("UU");
    identifier.set_station("MOUT");
    identifier.set_channel("HHZ");
    identifier.set_location_code("01");
    UV1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    constexpr double samplingRate{1}; // 1 sps helps with subsequet test on slow machine
    packet.set_sampling_rate(samplingRate);
    std::vector<int> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    packet.set_data(::pack(data));
    packet.set_number_of_samples(data.size());
    packet.set_data_type(dataType);
    constexpr std::chrono::microseconds maxFutureTime{1000};
    FuturePacketDetectorOptions options;
    options.setMaxFutureTime(maxFutureTime);
    FuturePacketDetector detector{options};

    SECTION("StartTime")
    {
        constexpr std::chrono::microseconds startTime{std::chrono::seconds {1}};
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 startTime.count());
        auto endTimeMuS = Utilities::getEndTimeInMicroSeconds(packet).count();
        auto referenceEndTimeMuS = startTime.count()
            + static_cast<int64_t>
              (std::round( (data.size() - 1)/samplingRate*1000000 ));
        REQUIRE(endTimeMuS == referenceEndTimeMuS);
    }
    SECTION("ValidData")
    {
        // 1970 better not be from the future
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(0);
        REQUIRE(detector.allow(packet));
    }
    auto now = std::chrono::high_resolution_clock::now();
    auto nowMuSeconds
        = std::chrono::time_point_cast<std::chrono::microseconds>
          (now).time_since_epoch();
    SECTION("FutureData")
    {
        // Low sampling rate will make this work even running on slow
        // computers.  Need like 9s to get to this test.
        auto startTime = nowMuSeconds - std::chrono::microseconds {100};
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(startTime.count());
        REQUIRE(!detector.allow(packet));
    }
    SECTION("Copy")
    {
        auto detectorCopy = detector;
        auto startTime = nowMuSeconds - std::chrono::microseconds {100};
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(startTime.count());
        REQUIRE(!detectorCopy.allow(packet));
    }
}

TEST_CASE("UDataPacketService::ExpiredPacketDetector", "[expiredData]")
{
    namespace UV1 = UDataPacketServiceAPI::V1;
    constexpr auto dataType{UV1::DataType::DATA_TYPE_INTEGER_32};
    UV1::StreamIdentifier identifier;
    identifier.set_network("UU");
    identifier.set_station("ELU");
    identifier.set_channel("EHZ");
    identifier.set_location_code("01");

    constexpr double samplingRate{100};
    UV1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    packet.set_sampling_rate(samplingRate);
    // N.B. valgrind runs too slow - can either lower the sampling rate or
    // make the packet longer
    std::vector<int64_t> packetData(100);
    std::iota(packetData.begin(), packetData.end(), 1);
    packet.set_number_of_samples(packetData.size());
    packet.set_data_type(dataType);
    packet.set_data(::pack(packetData));
    constexpr std::chrono::microseconds maxExpiredTime{10000}; // 0.01 seconds (packet duration is 0.99 s)
    ExpiredPacketDetectorOptions options;
    options.setMaxExpiredTime(maxExpiredTime);

    ExpiredPacketDetector detector{options};
    SECTION("ValidData")
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto nowMuSeconds
            = std::chrono::time_point_cast<std::chrono::microseconds>
              (now).time_since_epoch();
        auto startTime = nowMuSeconds - std::chrono::microseconds {100};
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 startTime.count());
        REQUIRE(detector.allow(packet)); // Fails in valgrind if packet is too small
    }
    SECTION("ExpiredData")
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto nowMuSeconds
            = std::chrono::time_point_cast<std::chrono::microseconds>
              (now).time_since_epoch();
        // Sometimes it executes too fast so we need to subtract a little
        // tolerance 
        auto startTime = std::chrono::microseconds {nowMuSeconds}
                       - maxExpiredTime
                       - std::chrono::microseconds{1};
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 startTime.count());
        REQUIRE(!detector.allow(packet));
    }
    SECTION("Copy")
    {
        auto detectorCopy = detector;
        auto now = std::chrono::high_resolution_clock::now();
        auto nowMuSeconds
            = std::chrono::time_point_cast<std::chrono::microseconds>
              (now).time_since_epoch();
        auto startTime = std::chrono::microseconds {nowMuSeconds}
                       - maxExpiredTime
                       - std::chrono::microseconds{1};
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 startTime.count());
        REQUIRE(!detectorCopy.allow(packet));
    }
}

TEST_CASE("UDataPacketService::DuplicatePacketDetector", "[duplicateData]")
{
    namespace UV1 = UDataPacketServiceAPI::V1;
    constexpr auto dataType{UV1::DataType::DATA_TYPE_INTEGER_32};
    // Random packet sizes
    std::random_device randomDevice;
    std::mt19937 generator(188382);
    std::uniform_int_distribution<> uniformDistribution(250, 350);

    // Define a base packet
    UV1::StreamIdentifier identifier;
    identifier.set_network("UU");
    identifier.set_station("CTU");
    identifier.set_channel("HHZ");
    identifier.set_location_code("01");

    const double samplingRate{100};
    UV1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    packet.set_sampling_rate(samplingRate); 

    // Define a start time
    auto now = std::chrono::high_resolution_clock::now();
    auto nowSeconds
        = std::chrono::time_point_cast<std::chrono::microseconds>
          (now).time_since_epoch();
    constexpr std::chrono::microseconds seconds600{600};
    auto startTime = nowSeconds - seconds600;

    // Business as usual - all data comes in on time and in order
    SECTION("All good data")
    {   
        const int circularBufferSize{15};

        DuplicatePacketDetectorOptions options;
        options.setCircularBufferSize(circularBufferSize);

        DuplicatePacketDetector detector{options};
        int cumulativeSamples{0}; 
        int nExamples = 2*circularBufferSize;
        for (int iPacket = 0; iPacket < nExamples; iPacket++)
        {
            auto packetStartTime = startTime 
                + std::chrono::microseconds {static_cast<int64_t>
                      (std::round(cumulativeSamples/samplingRate*1000000))};
            std::vector<int> data(uniformDistribution(generator), 0); 
            cumulativeSamples
                = cumulativeSamples + static_cast<int> (data.size()); 
            packet.set_number_of_samples(data.size());
            packet.set_data_type(dataType);
            packet.set_data(::pack(data));
            *packet.mutable_start_time()
                = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                     packetStartTime.count());
            REQUIRE(detector.allow(packet));
        }
    }   

    SECTION("Every other is a duplicate")
    {   
        const int circularBufferSize{15};

        DuplicatePacketDetectorOptions options;
        options.setCircularBufferSize(circularBufferSize);

        DuplicatePacketDetector detector{options};
        int cumulativeSamples{0}; 
        int nExamples = 2*circularBufferSize;
        for (int iPacket = 0; iPacket < nExamples; iPacket++)
        {
            auto packetStartTime = startTime 
                + std::chrono::microseconds {static_cast<int64_t>
                      (std::round(cumulativeSamples/samplingRate*1000000))};
            std::vector<int> data(uniformDistribution(generator), 0); 
            cumulativeSamples
                = cumulativeSamples + static_cast<int> (data.size()); 
            packet.set_number_of_samples(data.size());
            packet.set_data_type(dataType);
            packet.set_data(::pack(data));
            *packet.mutable_start_time()
                = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                     packetStartTime.count());
            CHECK(detector.allow(packet));
            CHECK(!detector.allow(packet));
        }
    }

    SECTION("Out of order with duplicates")
    {
        const int circularBufferSize{15};

        DuplicatePacketDetectorOptions options;
        options.setCircularBufferSize(circularBufferSize);

        DuplicatePacketDetector detector{options};

        std::vector<UV1::Packet> packets;
        int cumulativeSamples{0};
        for (int iPacket = 0; iPacket < circularBufferSize; iPacket++)
        {
            auto packetStartTime = startTime 
                + std::chrono::microseconds {static_cast<int64_t>
                      (std::round(cumulativeSamples/samplingRate*1000000))};
            std::vector<int> data(uniformDistribution(generator), 0);
            cumulativeSamples
                = cumulativeSamples + static_cast<int> (data.size());
            packet.set_number_of_samples(data.size());
            packet.set_data_type(dataType);
            packet.set_data(::pack(data));
            *packet.mutable_start_time()
                = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                     packetStartTime.count());
            packets.push_back(packet);
        }
        std::shuffle(packets.begin(), packets.end(), generator);

        for (const auto &outOfOrderPacket : packets)
        {
            //std::cout << std::setprecision(16) << "hey " << outOfOrderPacket.getStartTime().count()*1.e-6 << std::endl;
            REQUIRE(detector.allow(outOfOrderPacket));
        }
    }

    SECTION("Timing slips")
    {   
        const int circularBufferSize{15};

        DuplicatePacketDetectorOptions options;
        options.setCircularBufferSize(circularBufferSize);

        DuplicatePacketDetector detector{options};

        int cumulativeSamples{0}; 
        // Load it
        int nExamples = circularBufferSize;
        std::vector<UV1::Packet> packets;
        for (int iPacket = 0; iPacket < nExamples; iPacket++)
        {   
            auto packetStartTime = startTime 
                + std::chrono::microseconds {static_cast<int64_t>
                      (std::round(cumulativeSamples/samplingRate*1000000))};
            std::vector<int> data(uniformDistribution(generator), 0); 
            cumulativeSamples
                = cumulativeSamples + static_cast<int> (data.size());
            packet.set_number_of_samples(data.size());
            packet.set_data_type(dataType);
            packet.set_data(::pack(data));
            *packet.mutable_start_time()
                = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                     packetStartTime.count());
            CHECK(detector.allow(packet));
            packets.push_back(packet);
        }   
        REQUIRE(static_cast<int> (packets.size()) == nExamples);

        // Throw some timing slips in there
        auto firstPacket = packets.front();
        auto firstStartTimePerturbed
               = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
                   firstPacket.start_time())*1.e-6
               - (firstPacket.number_of_samples() - 1)
                /firstPacket.sampling_rate()/2.0;
        auto firstStartTimePerturbedMuS
            = static_cast<int64_t>
              (std::round(firstStartTimePerturbed*1000000));
        *firstPacket.mutable_start_time()
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 firstStartTimePerturbedMuS);
        CHECK(!detector.allow(firstPacket));
        for (int iPacket = 0; iPacket < nExamples; iPacket++)
        {   
            auto thisPacket = packets.at(iPacket);
            auto thisStartTimePerturbed
                = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
                     thisPacket.start_time())*1.e-6
                   - (thisPacket.number_of_samples() - 1)
                     /thisPacket.sampling_rate()/2.0;
            auto thisStartTimePerturbedMuS
                = static_cast<int64_t>
                  (std::round(thisStartTimePerturbed*1000000));
            *thisPacket.mutable_start_time()
                = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                      thisStartTimePerturbedMuS);
            //double packetStartTime = thisPacket.getStartTime().count()*1.e-6
            //                       + (thisPacket.getNumberOfSamples() - 1)
            //                         /thisPacket.getSamplingRate()/2;
            //thisPacket.setStartTime(packetStartTime);
            CHECK(!detector.allow(thisPacket));
        }
    }   
}

