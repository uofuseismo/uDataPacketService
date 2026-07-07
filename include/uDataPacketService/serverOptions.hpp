#ifndef UDATA_PACKET_SERVICE_SERVER_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_SERVER_OPTIONS_HPP
#include <chrono>
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

    /// @brief Sets the deadline given to grpc::Server::Shutdown.  In-flight
    ///        RPCs that have not completed by this deadline are forcibly
    ///        cancelled.
    /// @throws std::invalid_argument if the deadline is not positive.
    /// @note Side effect: if the deadline is less than the maximum writer
    ///       poll interval then the poll interval is lowered to the deadline
    ///       so a dozing writer always notices shutdown before the server
    ///       starts forcibly cancelling RPCs.
    void setShutdownDeadline(const std::chrono::milliseconds &deadline);
    /// @result The server shutdown deadline.  By default 1 second.
    [[nodiscard]] std::chrono::milliseconds getShutdownDeadline() const noexcept;

    /// @brief Sets the longest interval an idle subscriber write-reactor
    ///        may sleep between polls for new packets.  The reactor backs
    ///        off towards this while a stream is quiet.
    /// @throws std::invalid_argument if the interval is not positive or is
    ///         greater than or equal to the current shutdown deadline.
    void setMaximumWriterPollInterval(const std::chrono::milliseconds &interval);
    /// @result The maximum writer poll interval.  By default 250 ms.
    [[nodiscard]] std::chrono::milliseconds getMaximumWriterPollInterval() const noexcept;

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
