module;
#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/packet.pb.h"

export module Utilities;

namespace UDataPacketService::Utilities
{

[[nodiscard]] std::string toName(
    const UDataPacketServiceAPI::V1::StreamIdentifier &streamIdentifier)
{
    auto result = streamIdentifier.network() + "." 
                + streamIdentifier.station() + "." 
                + streamIdentifier.channel() + ".";
    if (!streamIdentifier.location_code().empty())
    {   
         result = result + "." + streamIdentifier.location_code();
    }   
    return result;
}

export
[[nodiscard]] 
std::string toName(const UDataPacketServiceAPI::V1::Packet &packet)
{
    return toName(packet.stream_identifier());
}

export
template<typename T>
[[nodiscard]] T getNow() 
{
    auto now 
       = std::chrono::duration_cast<T>
         ((std::chrono::high_resolution_clock::now()).time_since_epoch());
    return now;
}

export
[[nodiscard]] std::chrono::microseconds 
    getEndTimeInMicroSeconds(const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto startTimeMuSec
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
             packet.start_time());
    auto nSamples = packet.number_of_samples();
    auto samplingRate = packet.sampling_rate();
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }
    auto dtMuSec = static_cast<int64_t> (std::round(1000000/samplingRate));
    auto endTimeMuSec = startTimeMuSec = dtMuSec*(nSamples - 1);
    return std::chrono::microseconds {endTimeMuSec};
}

}


