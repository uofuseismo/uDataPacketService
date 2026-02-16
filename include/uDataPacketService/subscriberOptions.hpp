#ifndef UDATA_PACKET_SERVICE_SUBSCRIBER_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIBER_OPTIONS_HPP
#include <string>
#include <vector>
#include <memory>
namespace UDataPacketService
{
 class GRPCOptions;
}
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

    /// @brief GRPC connection options.
    void setGRPCOptions(const GRPCOptions &options);
    /// @result The gRPC connection options.
    [[nodiscard]] GRPCOptions getGRPCOptions() const;

    /// @brief Sets the reconnection schedule.
    void setReconnectSchedule(const std::vector<std::chrono::milliseconds> &reconnectSchedule);
    /// @result The reconnection schedule.
    [[nodiscard]] std::vector<std::chrono::milliseconds> getReconnectSchedule() const noexcept;

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
