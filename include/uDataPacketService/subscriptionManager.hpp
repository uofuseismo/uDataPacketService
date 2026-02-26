#ifndef UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_HPP
#include <memory>
#include <set>
#include <vector>
#include <spdlog/spdlog.h>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
 class StreamIdentifier;
}
namespace UDataPacketService
{
 class SubscriptionManagerOptions;
}
namespace UDataPacketService
{
/// @class SubscriptionManager "subscriptionManager.hpp"
/// @brief A subscription manager is a higher level tool for managing multiple
///        subscribers who wish to data from specific streams.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class SubscriptionManager
{
public:
    SubscriptionManager(const SubscriptionManagerOptions &options,
                        std::shared_ptr<spdlog::logger> logger);

    /// @name Publishers
    /// @{
 
    /// @brief Enqueues the next packet for consumption by all interested
    ///        subscribers.
    /// @param[in] packet  The packet to enqueue.
    /// @note If the stream corresponding to this packet does not exist then
    ///       it will be created.
    void enqueuePacket(const UDataPacketServiceAPI::V1::Packet &packet);
    /// @brief Enqueues the next packet for consumption by all interested
    ///        subscribers.
    /// @param[in,out] packet  The packet to enqueue.  On exit, packet's
    ///                        behavior is undefined.
    /// @note If the stream corresponding to this packet does not exist then
    ///       it will be created.
    void enqueuePacket(UDataPacketServiceAPI::V1::Packet &&packet);
    /// @}

    /// @name Subscribers
    /// @{

    /// @brief Subscribes to selected streams.
    /// @param[in] contextAddress  The RPC's memory location.
    /// @param[in] streamIdentifiers  The stream identifiers to which to subscribe.
    /// @throws std::invalid_argumetn if streamIdentifiers is empty.
    void subscribe(uintptr_t contextAddress,
                   const std::set<UDataPacketServiceAPI::V1::StreamIdentifier> &streamIdentifiers);

    /// @brief Subscribes to all streams.
    /// @param[in] serverContext  The server context.
    //template<typename U> void subscribeToAll(U *serverContext);
    /// @brief Subscribes to all streams.
    /// @param[in] contextAddress  The RPC's memory location.
    void subscribeToAll(uintptr_t contextAddress);

    /// @brief Gets the next packets from the streams to which I'm subscribed.
    /// @param[in] contextAddress  The RPC's memory address.
    /// @result The next batch of received packets.
    [[nodiscard]] std::vector<UDataPacketServiceAPI::V1::Packet> getPackets(uintptr_t contextAddress) const;

    /// @brief Subscribe to a subset of streams.
    /// @param[in] 
    /// @brief Unsubscribes the server context from all subscriptions.
    /// @param[in] serverContext  The server context.
    template<typename U> void unsubscribeFromAll(U *serverContext);
    void unsubscribeFromAll(uintptr_t contextAddress);
    /// @}
 
    /// @brief Destructor.
    ~SubscriptionManager();

    SubscriptionManager() = delete;
    SubscriptionManager(const SubscriptionManager &) = delete;
    SubscriptionManager(SubscriptionManager &&) noexcept = delete;
private:
    class SubscriptionManagerImpl;
    std::unique_ptr<SubscriptionManagerImpl> pImpl;
};
}
#endif
