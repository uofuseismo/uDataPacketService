#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <google/protobuf/util/time_util.h>
#include <boost/algorithm/string/trim.hpp>
#include "uDataPacketService/utilities.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/data_type.pb.h"
#include "uDataPacketImportAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/stream_identifier.pb.h"
#include "uDataPacketImportAPI/v1/data_type.pb.h"
//#include "uDataPacketImportAPI/v1/packet.pb.h"

namespace
{

void trimAndCapitalize(std::string &s)
{
    boost::algorithm::trim(s);
    if (!std::all_of(s.begin(), s.end(), [](unsigned char c)
        {
           return std::isupper(c); })) 
    {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    } 
}

[[nodiscard]]
UDataPacketServiceAPI::V1::StreamIdentifier convert(
    //const UDataPacketImportAPI::V1::StreamIdentifier &input)
    UDataPacketImportAPI::V1::StreamIdentifier &&input)
{
    UDataPacketServiceAPI::V1::StreamIdentifier result;
    auto network = std::move(*input.mutable_network());
    //auto network = input.network();
    trimAndCapitalize(network);
    if (network.empty()){throw std::invalid_argument("Network is empty");}
    result.set_network(std::move(network));

    auto station = std::move(*input.mutable_station());
    //auto station = input.station(); 
    trimAndCapitalize(station);
    if (station.empty()){throw std::invalid_argument("Station is empty");}
    result.set_station(std::move(station));

    auto channel = std::move(*input.mutable_channel());
    //auto channel = input.channel();
    trimAndCapitalize(channel);
    if (channel.empty()){throw std::invalid_argument("Channel is empty");}
    result.set_channel(std::move(channel));

    auto locationCode = std::move(*input.mutable_location_code());
    //auto locationCode = input.location_code();
    trimAndCapitalize(locationCode);
    if (locationCode.empty())
    {   
        result.set_location_code("--");
    }   
    else
    {
        result.set_location_code(std::move(locationCode));
    }
    return result;
}

}

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

UDataPacketServiceAPI::V1::Packet UDataPacketService::Utilities::convert(
    UDataPacketImportAPI::V1::Packet &&input)
{
    UDataPacketServiceAPI::V1::Packet result;

    // Quick/easy checks
    if (!input.has_stream_identifier())
    {
        throw std::invalid_argument("No stream identifier set on input packet");
    }
    if (!input.has_start_time())
    {
        throw std::invalid_argument("Start time not set on input packet");
    }
    if (!input.has_sampling_rate())
    {
        throw std::invalid_argument("Sampling rate not set on input packet");
    }
    if (!input.has_number_of_samples())
    {
        throw std::invalid_argument("Number of samples set on input packet");
    }
    if (!input.has_data_type())
    {
        throw std::invalid_argument("Data type not set on input packet");
    }
    if (!input.has_data())
    {
        throw std::invalid_argument("Data not set on input packet");
    }
    // Packet identifier
    //*result.mutable_stream_identifier()  
    //    = convert(input.stream_identifier());
    *result.mutable_stream_identifier() = ::convert(
        std::move(*input.mutable_stream_identifier()));

    // Number of samples
    auto nSamples = input.number_of_samples();
    if (nSamples <= 0){throw std::invalid_argument("No data in packet");}

    // Sampling rate
    result.set_number_of_samples(nSamples);
    const double samplingRate = input.sampling_rate();
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }
    result.set_sampling_rate(samplingRate);

    // Start time
    *result.mutable_start_time() = std::move(*input.mutable_start_time());

    // Data type and data
    auto dataType = input.data_type();
    if (dataType == UDataPacketImportAPI::V1::DataType::DATA_TYPE_INTEGER_32)
    {
        result.set_data_type(
            UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_32);
    }
    else if (dataType ==
             UDataPacketImportAPI::V1::DataType::DATA_TYPE_FLOAT)
    {
        result.set_data_type(
            UDataPacketServiceAPI::V1::DataType::DATA_TYPE_FLOAT);
    }
    else if (dataType ==
             UDataPacketImportAPI::V1::DataType::DATA_TYPE_DOUBLE)
    {
        result.set_data_type(
            UDataPacketServiceAPI::V1::DataType::DATA_TYPE_DOUBLE);
    }
    else if (dataType ==
             UDataPacketImportAPI::V1::DataType::DATA_TYPE_INTEGER_64)
    {
        result.set_data_type(
            UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_64);
    }
    else if (dataType ==
             UDataPacketImportAPI::V1::DataType::DATA_TYPE_TEXT)
    {
        result.set_data_type(
            UDataPacketServiceAPI::V1::DataType::DATA_TYPE_TEXT);
    }
    else
    {
        if (dataType ==
            UDataPacketImportAPI::V1::DataType::DATA_TYPE_UNKNOWN)
        {
            throw std::invalid_argument("Cannot process unknown data type");
        }
        throw std::runtime_error("Unhandled data type");
    }
    //result.set_data(std::move(*input.mutable_data()));
    std::swap(*result.mutable_data(), *input.mutable_data());
    
    return result;
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

