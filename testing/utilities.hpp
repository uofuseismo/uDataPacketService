#ifndef TESTING_UTILITIES_HPP
#define TESTING_UTILITIES_HPP
#include <bit>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"

namespace
{
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

template<typename T>
std::string pack(const std::vector<T> &data, const bool swapBytes)
{
    return pack(data.data(), data.size(), swapBytes);
}

template<typename T>
std::string pack(const std::vector<T> &data)
{
    const bool swapBytes
    {
        std::endian::native == std::endian::little ? false : true
    };
    return pack(data, swapBytes);
}

[[maybe_unused]] [[nodiscard]]
std::chrono::microseconds getNowSimple() 
{
     auto now 
        = std::chrono::duration_cast<std::chrono::seconds>
          ((std::chrono::high_resolution_clock::now()).time_since_epoch());
     return now;
}

namespace UV1 = UDataPacketServiceAPI::V1;
UDataPacketServiceAPI::V1::StreamIdentifier
     toIdentifier(const std::string &network,
                  const std::string &station,
                  const std::string &channel,
                  const std::string &locationCode)
{
    namespace UV1 = UDataPacketServiceAPI::V1;
    UV1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);
    return identifier;
}

[[maybe_unused]] [[nodiscard]]
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
    auto identifier = ::toIdentifier(network, station, channel, locationCode);

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

[[nodiscard]] [[maybe_unused]]
bool comparePacket(const UDataPacketServiceAPI::V1::Packet &lhs,
                   const UDataPacketServiceAPI::V1::Packet &rhs)
{
    if (lhs.number_of_samples() != rhs.number_of_samples())
    {
        return false;
    }
    if (lhs.stream_identifier().network() !=
        rhs.stream_identifier().network())
    {
        return false;
    }
    if (lhs.stream_identifier().station() !=
        rhs.stream_identifier().station())
    {
        return false;
    }
    if (lhs.stream_identifier().channel() !=
        rhs.stream_identifier().channel())
    {
        return false;
    }
    if (lhs.stream_identifier().location_code() !=
        rhs.stream_identifier().location_code())
    {
        return false;
    }
    if (std::abs(lhs.sampling_rate() - rhs.sampling_rate()) >
        1.e-14)
    {
        return false;
    }
    if (lhs.start_time() != rhs.start_time())
    {
        return false;
    }
    if (lhs.data_type() != rhs.data_type()){return false;}
    if (lhs.data() != rhs.data()){return false;}
    return true;
}

[[nodiscard]] [[maybe_unused]]
bool comparePackets(const std::vector<UDataPacketServiceAPI::V1::Packet> &lhs,
                    const std::vector<UDataPacketServiceAPI::V1::Packet> &rhs,
                    bool ordered = true)
{
    if (lhs.size() != rhs.size()){return true;}
    auto nPackets = static_cast<int> (lhs.size());
    for (int i = 0; i < nPackets; ++i)
    {
        if (ordered)
        {
            bool matched{false};
            for (int j = 0; j < nPackets; ++j)
            {
                if (comparePacket(lhs.at(i), rhs.at(j)))
                {
                    matched = true;
                    break;
                }
            }
            if (!matched){return false;}
        }
        else
        {
            if (!comparePacket(lhs.at(i), rhs.at(i)))
            {
                return false;
            }
        }
    }   
    return true;
}


}
#endif
