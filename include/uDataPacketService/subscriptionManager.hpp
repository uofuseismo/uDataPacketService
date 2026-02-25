#ifndef UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_HPP
#include <memory>
namespace UDataPacketService
{
/// @brief A subscription manager is a higher level tool for managing multiple
///        subscribers and streams.  This simplifies the logic of the RPCs
///        where a subscriber looks to subscribe to all streams, or a subset
///        of streams, etc.
class SubscriptionManager
{
public:
    ~SubscriptionManager();
private:
    class SubscriptionManagerImpl;
    std::unique_ptr<SubscriptionManagerImpl> pImpl;
};
}
#endif
