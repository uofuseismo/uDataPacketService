#ifndef UDATA_PACKET_SERVICE_GRPC_CLIENT_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_GRPC_CLIENT_OPTIONS_HPP
#include <string>
#include <memory>
#include <filesystem>
#include <optional>
namespace UDataPacketService
{
/// @class GRPCClientOptions 
/// @brief Defines the gRPC client options.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class GRPCClientOptions
{
public:
    /// @brief Constructor
    GRPCClientOptions();

    /// @brief Sets the host name - e.g., localhost or machine.domain.com.
    void setHost(const std::string &host);
    /// @result The host name.
    /// @note By default this is localhost. 
    [[nodiscard]] std::string getHost() const noexcept;
  
    /// @brief Sets the port number.
    void setPort(uint16_t port);
    /// @result The port.
    /// @note By default this is 50000.
    [[nodiscard]] uint16_t getPort() const noexcept;

    /// @brief Sets the API access token.
    void setAccessToken(const std::string &token);
    /// @result The access token.
    /// @note Access tokens can only be used by gRPC if the server
    ///       certificate is set.
    [[nodiscard]] std::optional<std::string> getAccessToken() const noexcept;

    /// @brief Sets the server's certificate.  This is public - e.g.,
    ///        localhost.crt.
    void setServerCertificate(const std::string &certificate);
    /// @result The server certificate.
    /// @note The server key must also be set for gRPC to use this.
    [[nodiscard]] std::optional<std::string> getServerCertificate() const noexcept;

    /// @brief Sets the client certificate for a full key exchange.
    void setClientCertificate(const std::string &certificate);
    /// @result The client ceritificate.
    [[nodiscard]] std::optional<std::string> getClientCertificate() const noexcept;

    /// @brief Sets the client's key.  This is private - e.g., localhost.key.
    void setClientKey(const std::string &key);
    /// @result The client key.
    /// @note The client certificate must also be set for gRPC to use this.
    [[nodiscard]] std::optional<std::string> getClientKey() const noexcept;

    /// @brief Destructor
    ~GRPCClientOptions();
    /// @brief Copy constructor.
    GRPCClientOptions(const GRPCClientOptions &options);
    /// @brief Move constructor.
    GRPCClientOptions(GRPCClientOptions &&options) noexcept;
    /// @brief Copy assignment.
    GRPCClientOptions& operator=(const GRPCClientOptions &options);
    /// @brief Move assignment.
    GRPCClientOptions& operator=(GRPCClientOptions &&options) noexcept;
private:
    class GRPCClientOptionsImpl;
    std::unique_ptr<GRPCClientOptionsImpl> pImpl;
};
/// @brief Convenience function to convert host and port to an address for gRPC.
[[nodiscard]] std::string makeAddress(const GRPCClientOptions &options);
}
#endif
