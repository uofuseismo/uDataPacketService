#ifndef UDATA_PACKET_SERVICE_SERVER_HPP
#define UDATA_PACKET_SERVICE_SERVER_HPP
#include <memory>
#include <spdlog/spdlog.h>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}
namespace UDataPacketService
{
 class ServerOptions;
}
namespace UDataPacketService
{
/// @class Server server.hpp
/// @brief The server manages the underlying service.  The service is responsible
///        to sending packets to clients.
/// @copyright Ben Baker (University of Utah) distributed under the MIT
///            NO AI license.
class Server
{
public:
    /// @brief Constructs the server from the given options and logger.
    Server(const ServerOptions &options, std::shared_ptr<spdlog::logger> logger);

    /// @brief Starts the server.
    void start();
    /// @result The current number of subscribers.
    [[nodiscard]] int getNumberOfSubscribers() const noexcept;
    /// @brief Stops the server
    void stop();

    /// @brief Allows a data source to publish a packet.
    void enqueuePacket(UDataPacketServiceAPI::V1::Packet &&packet); 
    /// @brief Allows a data source to publish a packet.
    void enqueuePacket(const UDataPacketServiceAPI::V1::Packet &packet);

    /// @brief Destructor
    ~Server();

    Server() = delete;
    Server(const Server &) = delete;
    Server(Server &&) noexcept = delete;
    Server& operator=(const Server &) = delete;
    Server& operator=(Server &&) noexcept = delete;
private:
    class ServerImpl;
    std::unique_ptr<ServerImpl> pImpl;
};
}
#endif
