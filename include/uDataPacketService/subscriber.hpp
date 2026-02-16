#ifndef UDATA_PACKET_SERVICE_SUBSCRIBER_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIBER_HPP
#include <memory>
#include <functional>
namespace UDataPacketService
{
 class SubscriberOptions;
}
namespace UDataPacketService
{
/// @brief Receives packets from the the uDataPacketImportProxy backend
///        broadcast.
/// @copyright Ben Baker (University Utah of Utah) distributed under the MIT
///            NO AI license.
class Subscriber
{
public:
    /// @brief Copy constructor.
    
    /// @brief Destructor
    ~Subscriber();

    Subscriber() = delete;
private:
    class SubscriberImpl;
    std::unique_ptr<SubscriberImpl> pImpl;

};
}
#endif
