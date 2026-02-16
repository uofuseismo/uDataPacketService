module;
#include <algorithm>
#include <string>
#include <boost/algorithm/string/trim.hpp>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketImportAPI/v1/packet.pb.h"

export module PacketConverter;

namespace UDataPacketService
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
    UDataPacketImportAPI::V1::StreamIdentifier &&input)
{
    UDataPacketServiceAPI::V1::StreamIdentifier result;
    auto network = std::move(*input.mutable_network());
    trimAndCapitalize(network);
    if (network.empty()){throw std::invalid_argument("Network is empty");}
    result.set_network(std::move(network));

    auto station = std::move(*input.mutable_station());
    trimAndCapitalize(station);
    if (station.empty()){throw std::invalid_argument("Station is empty");}
    result.set_station(std::move(station));

    auto channel = std::move(*input.mutable_channel());
    trimAndCapitalize(channel);
    if (channel.empty()){throw std::invalid_argument("Channel is empty");}
    result.set_channel(std::move(channel));

    auto locationCode = std::move(*input.mutable_location_code());
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
 
export
[[nodiscard]]
UDataPacketServiceAPI::V1::Packet convert(
    UDataPacketImportAPI::V1::Packet &&input)
{
    UDataPacketServiceAPI::V1::Packet result;

    // Packet identifier
    *result.mutable_stream_identifier() = convert(
        std::move(*input.mutable_stream_identifier()));

    // Number of samples
    auto nSamples = input.number_of_samples();
    if (nSamples <= 0){throw std::invalid_argument("No data in packet");}

    // Sampling rate
    result.set_number_of_samples(nSamples);
    double samplingRate = input.sampling_rate();
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }
    result.set_sampling_rate(samplingRate);

    // Start time
    *result.mutable_start_time() = input.start_time();

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
    result.set_data(std::move(*input.mutable_data()));
    
    return result;
}

}
