#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketService/utilities.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
//#include "uDataPacketImportAPI/v1/packet.pb.h"



std::string UDataPacketService::Utilities::toName(
    const UDataPacketServiceAPI::V1::StreamIdentifier &streamIdentifier)
{
    if (!streamIdentifier.has_network())
    {
        throw std::invalid_argument("Network not set");
    }
    if (!streamIdentifier.has_station())
    {
        throw std::invalid_argument("Station not set");
    }
    if (!streamIdentifier.has_channel())
    {
        throw std::invalid_argument("Channel not set");
    }
    auto result = streamIdentifier.network() + "." 
                + streamIdentifier.station() + "." 
                + streamIdentifier.channel();
    if (streamIdentifier.has_location_code())
    {
        if (!streamIdentifier.location_code().empty())
        {   
            result = result + "." + streamIdentifier.location_code();
        }   
    }
    return result;
}

std::string UDataPacketService::Utilities::toName(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Stream identifier not set");
    }
    return toName(packet.stream_identifier());
}

namespace
{

template<typename T>
[[nodiscard]] T getNow() 
{
    auto now 
       = std::chrono::duration_cast<T>
         ((std::chrono::high_resolution_clock::now()).time_since_epoch());
    return now;
}

}

template<>
std::chrono::nanoseconds UDataPacketService::Utilities::getNow()
{
    return ::getNow<std::chrono::nanoseconds> ();
}

template<>
std::chrono::microseconds UDataPacketService::Utilities::getNow()
{
    return ::getNow<std::chrono::microseconds> ();
}

template<>
std::chrono::seconds UDataPacketService::Utilities::getNow()
{
    return ::getNow<std::chrono::seconds> ();
}

std::chrono::microseconds 
    UDataPacketService::Utilities::getEndTimeInMicroSeconds(
        const UDataPacketServiceAPI::V1::Packet &packet)
{
    if (!packet.has_start_time())
    {
        throw std::invalid_argument("Start time not set");
    }
    if (!packet.has_sampling_rate())
    {
        throw std::invalid_argument("Sampling rate not set");
    }
    if (!packet.has_number_of_samples())
    {
        throw std::invalid_argument("Number of samples not set");
    }
    auto startTimeMuSec
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
             packet.start_time());
    auto nSamples = packet.number_of_samples();
    if (nSamples == 0)
    {
        return std::chrono::microseconds {startTimeMuSec};
    }
    const double samplingRate = packet.sampling_rate();
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }
    auto dtMuSec = static_cast<int64_t> (std::round(1000000/samplingRate));
    auto endTimeMuSec = startTimeMuSec + dtMuSec*(nSamples - 1);
    return std::chrono::microseconds {endTimeMuSec};
}


namespace UDataPacketServiceAPI::V1
{

[[nodiscard]] 
bool operator<(const StreamIdentifier &lhs, const StreamIdentifier &rhs)
{
    return UDataPacketService::Utilities::toName(lhs) <
           UDataPacketService::Utilities::toName(rhs);
}


}

