#ifndef UDATA_PACKET_SERVICE_STREAM_HPP
#define UDATA_PACKET_SERVICE_STREAM_HPP
#include <set>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}
namespace UDataPacketService
{
class StreamOptions;
}
namespace UDataPacketService
{
/// @class Stream "stream.hpp"
/// @brief A seismic stream is a stream of packetized data generated
///        by unique network, station, channel, location tuple.
///        Subscribers subscribe to streams.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Stream
{
public:
    /// @name Publisher
    /// @{

    /// @brief Constructs a stream from a packet.  No logging will be done.
    Stream(UDataPacketServiceAPI::V1::Packet &&packet,
           const StreamOptions &options);
    /// @brief Constructs a stream from a packet with a logger.
    Stream(UDataPacketServiceAPI::V1::Packet &&packet,
           const StreamOptions &options,
           std::shared_ptr<spdlog::logger> logger);

    /// @brief Sets the next packet for all subscribers.
    /// @param[in,out] packet  The packet to add.  On exit, packet's behavior
    ///                        is undefined.
    /// @throws std::invalid_argument if the packet's stream identifier does
    ///         not match this stream's identifier.
    void setNextPacket(UDataPacketServiceAPI::V1::Packet &&packet);
    /// @brief Sets the next packet for all subscribers.
    /// @param[in] packet  The packet to add.
    /// @throws std::invalid_argument if the packet's stream identifier does
    ///         not match this stream's identifier.
    void setNextPacket(const UDataPacketServiceAPI::V1::Packet &packet);

    /// @}

    /// @name Subscriber
    /// @{

    /// @brief Subscribes to the stream.
    /// @param[in] contextAddress  The identifier to subscribe.
    /// @param[in] enqeueuLatestPacket  If true then if the most recent packet
    ///                                 is available then the subscriber will
    ///                                 be able to access it.  Otherwise, 
    ///                                 the subscriber's first packet will be
    ///                                 the next packet enqueued by the
    ///                                 publisher.
    /// @result True indicates the subscription was successful.
    [[nodiscard]] bool subscribe(uintptr_t contextAddress,
                                 bool enqueueLatestPacket);

    /// @brief Subscriber gets next packet
    /// @param[in] contextAddress  The subscriber's identifier.
    /// @result The next packet if it exists.  Otherwise, std::nullopt.
    [[nodiscard]] std::optional<UDataPacketServiceAPI::V1::Packet>
        getNextPacket(uintptr_t contextAddress) noexcept;

    /// @brief Unsubscribes from the stream.
    /// @param[in] contextAddress  The identifier to unsubscribe.
    /// @result True indicates the context was subscribed and is not
    ///         unsubscribed.
    [[nodiscard]] bool unsubscribe(uintptr_t contextAddress);

    /// @result The stream identifier.
    [[nodiscard]] std::string getIdentifier() const noexcept;
    /// @}

    /// @name Management
    /// @{

    /// @result The number of subscribers.
    [[nodiscard]] int getNumberOfSubscribers() const noexcept;
    /// @result The current subscribers.
    [[nodiscard]] std::set<uintptr_t> getSubscribers() const noexcept;
    /// @result True indicates this subscriber is subscribed.
    [[nodiscard]] bool isSubscribed(uintptr_t contextAddress) const noexcept;
    /// @}

    /// @brief Destructor
    ~Stream();

    Stream() = delete;
    Stream(const Stream &) = delete;
    Stream(Stream &&) noexcept = delete;
    Stream& operator=(const Stream &) = delete;
    Stream& operator=(Stream &&) noexcept = delete;
private:
    class StreamImpl;
    std::unique_ptr<StreamImpl> pImpl;
};
}
#endif
