#ifndef UDATA_PACKET_SERVICE_SUBSCRIBER_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIBER_HPP
#include <memory>
#include <future>
#include <functional>
namespace UDataPacketImportAPI::V1
{
 class Packet;
}
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
    /// @brief Subscriber
    Subscriber(const SubscriberOptions &options,
               const std::function<void (UDataPacketImportAPI::V1::Packet &&)> &callback,
               std::shared_ptr<spdlog::logger> logger); 
    
    /// @brief Destructor
    ~Subscriber();

    Subscriber() = delete;
    Subscriber(const Subscriber &) = delete;
    Subscriber(Subscriber &&) noexcept = delete;
private:
    class SubscriberImpl;
    std::unique_ptr<SubscriberImpl> pImpl;

};
}
#endif
