#ifndef UDATA_PACKET_SERVICE_SERVER_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_SERVER_OPTIONS_HPP
#include <string>
#include <vector>
#include <memory>
namespace UDataPacketService
{
 class GRPCServerOptions;
 class SubscriptionManagerOptions;
}
namespace UDataPacketService
{
/// @class ServerOptions
/// @brief The options for the gRPC server.  The server streams packets
///        to clients.
/// @copyright Ben Baker (University of Utah) distributed under the MIT
///            NO AI license.
class ServerOptions
{
public:
    /// @brief Constructor.
    ServerOptions();
    /// @brief Copy constructor.
    ServerOptions(const ServerOptions &options);
    /// @brief Move constructor.
    ServerOptions(ServerOptions &&options) noexcept;

    /// @brief Sets the GRPC connection options.
    void setGRPCOptions(const GRPCServerOptions &options);
    /// @result The gRPC connection options.
    [[nodiscard]] GRPCServerOptions getGRPCOptions() const;

    /// @brief Sets the subscription manager options.
    void setSubscriptionManagerOptions(const SubscriptionManagerOptions &options);
    /// @result The  subscription manager options.
    [[nodiscard]] SubscriptionManagerOptions getSubscriptionManagerOptions() const;

    /// @brief Sets the maximum number of subscribers.
    /// @param[in] maxSubscribers   The maximum number of subscribers.
    void setMaximumNumberOfSubscribers(int maxSubscribers);
    /// @result The maximum number of subscribers.
    [[nodiscard]] int getMaximumNumberOfSubscribers() const noexcept;

    /// @brief Sets the subscriber identifier.
    //void setIdentifier(const std::string &name);
    /// @result The subscriber identifier.
    //[[nodiscard]] std::optional<std::string> getIdentifier() const noexcept;
    
    /// @brief Copy assignment.
    ServerOptions& operator=(const ServerOptions &options);
    /// @brief Move assignment.
    ServerOptions& operator=(ServerOptions &&options) noexcept;
    /// @brief Destructor.
    ~ServerOptions();
private:
    class ServerOptionsImpl;
    std::unique_ptr<ServerOptionsImpl> pImpl;
};
}
#endif
