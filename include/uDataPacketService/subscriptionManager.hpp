#ifndef UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_HPP
#include <memory>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}
namespace UDataPacketService
{
/// @class SubscriptionManager "subscriptionManager.hpp"
/// @brief A subscription manager is a higher level tool for managing multiple
///        subscribers and streams.  This simplifies the logic of the RPCs
///        where a subscriber looks to subscribe to all streams, or a subset
///        of streams, etc.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
template<typename T>
class SubscriptionManager
{
public:
SubscriptionManager();

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

    /// @brief Subscribes to all streams.
    /// @param[in] serverContext  The
    void subscribeToAll(T *serverContext);
    /// @brief Unsubscribes the server context from all subscriptions.
    /// @param[in] serverContext  The server context.
    void unsubscribeFromAll(T *serverContext);
    /// @}
 
    /// @brief Destructor.
    ~SubscriptionManager();
private:
    class SubscriptionManagerImpl;
    std::unique_ptr<SubscriptionManagerImpl> pImpl;
};
}
#endif
