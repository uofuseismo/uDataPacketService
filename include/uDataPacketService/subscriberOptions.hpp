#ifndef UDATA_PACKET_SERVICE_SUBSCRIBER_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIBER_OPTIONS_HPP
#include <string>
#include <memory>
namespace UDataPacketService
{
/// @class SubscriberOptions
/// @brief The options for the gRPC subscriber.  The subscriber streams packets
///        from the import proxy backend.
/// @copyright Ben Baker (University of Utah) distributed under the MIT
///            NO AI license.
class SubscriberOptions
{
public:
    /// @brief Constructor.
    SubscriberOptions();
    /// @brief Copy constructor.
    SubscriberOptions(const SubscriberOptions &options);
    /// @brief Move constructor.
    SubscriberOptions(SubscriberOptions &&options) noexcept;
    /// @brief Copy assignment.
    SubscriberOptions& operator=(const SubscriberOptions &options);
    /// @brief Move assignment.
    SubscriberOptions& operator=(SubscriberOptions &&options) noexcept;
    /// @brief Destructor.
    ~SubscriberOptions();
private:
    class SubscriberOptionsImpl;
    std::unique_ptr<SubscriberOptionsImpl> pImpl;
};
}
#endif
