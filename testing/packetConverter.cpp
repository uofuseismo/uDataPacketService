#include <cmath>
#include <string>
#include <chrono>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketService/grpcOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/packet.pb.h"
#include "utilities.hpp"

import PacketConverter;

namespace
{
/*
template<typename T>
std::string pack(const T *data, const int nSamples, const bool swapBytes)
{
    constexpr auto dataTypeSize = sizeof(T);
    std::string result;
    if (nSamples < 1){return result;}
    result.resize(dataTypeSize*nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        unsigned char cArray[dataTypeSize];
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                      result.data() + dataTypeSize*i);
        }
    }
    else
    {
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::reverse_copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                              result.data() + dataTypeSize*i);
        }
    }
    return result;
}
*/

template<typename T>
UDataPacketImportAPI::V1::DataType toDataType()
{
    if (std::is_same<T, int>::value || std::is_same<T, int32_t>::value)
    {
        return UDataPacketImportAPI::V1::DataType::DATA_TYPE_INTEGER_32;
    }
    else if (std::is_same<T, int64_t>::value)
    {
        return UDataPacketImportAPI::V1::DataType::DATA_TYPE_INTEGER_64;
    }
    else if (std::is_same<T, float>::value)
    {
        return UDataPacketImportAPI::V1::DataType::DATA_TYPE_FLOAT;
    }
    else if (std::is_same<T, double>::value)
    {
        return UDataPacketImportAPI::V1::DataType::DATA_TYPE_DOUBLE;
    }
    else if (std::is_same<T, char>::value)
    {
        return UDataPacketImportAPI::V1::DataType::DATA_TYPE_TEXT;
    }
    return UDataPacketImportAPI::V1::DataType::DATA_TYPE_UNKNOWN;
}

template<typename T>
UDataPacketServiceAPI::V1::DataType toOutputDataType()
{
    if (std::is_same<T, int>::value || std::is_same<T, int32_t>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_32;
    }   
    else if (std::is_same<T, int64_t>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_64;
    }   
    else if (std::is_same<T, float>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_FLOAT;
    }   
    else if (std::is_same<T, double>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_DOUBLE;
    }   
    else if (std::is_same<T, char>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_TEXT;
    }   
    return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_UNKNOWN;
}

}

TEMPLATE_TEST_CASE("UDataPacketService", "[packetConveter]",
                   int, float, double, int64_t)
{
    const bool swapBytes
    {
        std::endian::native == std::endian::little ? false : true
    };
    const std::string networkIn{"Uu"};  const std::string network{"UU"};
    const std::string stationIn{"CwU"}; const std::string station{"CWU"};
    const std::string channelIn{"HHz"}; const std::string channel{"HHZ"};
    const std::string locationCode{"01"};
    std::string packedData;
    int nSamples{-1};
    if constexpr (std::is_same<TestType, char>::value)
    {
        std::vector<TestType> data{'a', 'b', 'c', 'd', 'e', 'f', 'g'};
        nSamples = static_cast<int> (data.size());
        packedData = ::pack(data.data(), data.size(), swapBytes);
    }
    else
    {
        std::vector<TestType> data{1, 2, 3, 4, 5, 6, 7, 8}; 
        nSamples = static_cast<int> (data.size());
        packedData = ::pack(data.data(), data.size(), swapBytes);
    }
    const std::chrono::nanoseconds startTime{1769631059123321000};
    constexpr double samplingRate{99.9995}; 

    SECTION("With location code")
    {
        UDataPacketImportAPI::V1::StreamIdentifier importIdentifier;
        importIdentifier.set_network(network);
        importIdentifier.set_station(station);
        importIdentifier.set_channel(channel);
        importIdentifier.set_location_code(locationCode);

        UDataPacketImportAPI::V1::Packet importPacket;
        *importPacket.mutable_stream_identifier() = importIdentifier;
        *importPacket.mutable_start_time()
            = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
                 startTime.count());
        importPacket.set_sampling_rate(samplingRate);
        importPacket.set_number_of_samples(nSamples);
        importPacket.set_data_type(::toDataType<TestType> ());
        importPacket.set_data(packedData);

        auto outputPacket
            = UDataPacketService::convert(std::move(importPacket));

        REQUIRE(outputPacket.stream_identifier().network() == network);
        REQUIRE(outputPacket.stream_identifier().station() == station);
        REQUIRE(outputPacket.stream_identifier().channel() == channel);
        REQUIRE(outputPacket.stream_identifier().location_code() == 
                locationCode);

        REQUIRE(google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                  outputPacket.start_time()) == startTime.count());
        REQUIRE(std::abs(outputPacket.sampling_rate() - samplingRate) < 1.e-14);
        REQUIRE(outputPacket.data_type() == toOutputDataType<TestType> ());
        REQUIRE(static_cast<int> (outputPacket.number_of_samples())
                == nSamples);
        REQUIRE(outputPacket.data() == packedData);
        REQUIRE(outputPacket.data().size() == packedData.size());
    }

    SECTION("Without location code")
    {
        UDataPacketImportAPI::V1::StreamIdentifier importIdentifier;
        importIdentifier.set_network(network);
        importIdentifier.set_station(station);
        importIdentifier.set_channel(channel);
        //importIdentifier.set_location_code(locationCode);

        UDataPacketImportAPI::V1::Packet importPacket;
        *importPacket.mutable_stream_identifier() = importIdentifier;
        *importPacket.mutable_start_time()
            = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
                 startTime.count());
        importPacket.set_sampling_rate(samplingRate);
        importPacket.set_number_of_samples(nSamples);
        importPacket.set_data_type(::toDataType<TestType> ());
        importPacket.set_data(packedData);
    
        auto outputPacket
            = UDataPacketService::convert(std::move(importPacket));

        REQUIRE(outputPacket.stream_identifier().network() == network);
        REQUIRE(outputPacket.stream_identifier().station() == station);
        REQUIRE(outputPacket.stream_identifier().channel() == channel);
        REQUIRE(outputPacket.stream_identifier().location_code() == "--");

        REQUIRE(google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                  outputPacket.start_time()) == startTime.count());
        REQUIRE(std::abs(outputPacket.sampling_rate() - samplingRate) < 1.e-14);
        REQUIRE(outputPacket.data_type() == toOutputDataType<TestType> ());
        REQUIRE(static_cast<int> (outputPacket.number_of_samples()) ==
                nSamples);
        REQUIRE(outputPacket.data() == packedData);
        REQUIRE(outputPacket.data().size() == packedData.size());
    }
}

