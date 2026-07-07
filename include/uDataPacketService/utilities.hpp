#ifndef UDATA_PACKET_SERVICE_UTILITIES_HPP
#define UDATA_PACKET_SERVICE_UTILITIES_HPP
#include <chrono>
#include <string>
namespace UDataPacketServiceAPI::V1
{
 class StreamIdentifier;
 class Packet;
}
namespace UDataPacketService::Utilities
{

/// Converts a stream a identifier to a consistent name.
[[nodiscard]] std::string toName(const UDataPacketServiceAPI::V1::StreamIdentifier &streamIdentifier);
/// Converst the stream identifier in the packet to a consistent name.
[[nodiscard]] std::string toName(const UDataPacketServiceAPI::V1::Packet &packet);
/// @result The end time of the packet.
[[nodiscard]] std::chrono::microseconds 
    getEndTimeInMicroSeconds(const UDataPacketServiceAPI::V1::Packet &packet);


template<typename T> T getNow();
}
#endif
