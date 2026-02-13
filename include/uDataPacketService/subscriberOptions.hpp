#ifndef UDATA_PACKET_SERVICE_SUBSCRIBER_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIBER_OPTIONS_HPP
#include <string>
#include <memory>
namespace UDataPacketService
{
class SubscriberOptions
{
public:

private:
    class SubscriberOptionsImpl;
    std::unique_ptr<SubscriberOptionsImpl> pImpl;
};
}
#endif
