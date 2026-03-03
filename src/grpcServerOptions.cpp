#include <string>
#include <algorithm>
#include <filesystem>
#include "uDataPacketService/grpcServerOptions.hpp"

using namespace UDataPacketService;

class GRPCServerOptions::GRPCServerOptionsImpl
{
public:
    std::string mHost{"localhost"};
    std::string mAccessToken;
    std::string mServerCertificate;
    std::string mServerKey;
    std::string mClientCertificate;
    uint16_t mPort{50000};
    bool mHaveServerCertificate{false}; 
    bool mHaveServerKey{false};
    bool mHaveClientCertificate{false};
    bool mHaveAccessToken{false};
};

/// Constructor
GRPCServerOptions::GRPCServerOptions() :
    pImpl(std::make_unique<GRPCServerOptionsImpl> ())
{
}

/// Copy constructor
GRPCServerOptions::GRPCServerOptions(const GRPCServerOptions &options)
{
    *this = options;
}

/// Move constructor
GRPCServerOptions::GRPCServerOptions(GRPCServerOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
GRPCServerOptions& 
GRPCServerOptions::operator=(const GRPCServerOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<GRPCServerOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
GRPCServerOptions& 
GRPCServerOptions::operator=(GRPCServerOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
GRPCServerOptions::~GRPCServerOptions() = default; 

/// Host
void GRPCServerOptions::setHost(const std::string &hostIn)
{
    auto host = hostIn;
    host.erase(
       std::remove_if(host.begin(), host.end(), ::isspace),
       host.end());
    if (host.empty())
    {
        throw std::invalid_argument("Host is empty");
    }
    pImpl->mHost = host;
}

std::string GRPCServerOptions::getHost() const noexcept
{
    return pImpl->mHost;
}

std::string UDataPacketService::makeAddress(const GRPCServerOptions &options)
{
    return options.getHost() + ":" + std::to_string(options.getPort());
}

/// Port
void GRPCServerOptions::setPort(const uint16_t port)
{
    if (port < 1)
    {
        throw std::invalid_argument("port must be positive");
    }
    pImpl->mPort = port;
}

uint16_t GRPCServerOptions::getPort() const noexcept
{
    return pImpl->mPort;
}

/// Server cert
void GRPCServerOptions::setServerCertificate(const std::string &cert)
{
    if (cert.empty())
    {
        throw std::invalid_argument("Server certificate is empty");
    }
    pImpl->mHaveServerCertificate = true;
    pImpl->mServerCertificate = cert;
}

std::optional<std::string> GRPCServerOptions::getServerCertificate() const noexcept
{
    return pImpl->mHaveServerCertificate ? 
           std::make_optional<std::string> (pImpl->mServerCertificate) :
           std::nullopt;
}

void GRPCServerOptions::setServerKey(const std::string &key)
{   
    if (key.empty())
    {   
        throw std::invalid_argument("Server key is empty");
    }
    pImpl->mHaveServerKey = true;
    pImpl->mServerKey = key;
}

std::optional<std::string> GRPCServerOptions::getServerKey() const noexcept
{
    return pImpl->mHaveServerKey ? 
           std::make_optional<std::string> (pImpl->mServerKey) :
           std::nullopt;
}

/// Client cert
void GRPCServerOptions::setClientCertificate(const std::string &cert)
{
    if (cert.empty())
    {   
        throw std::invalid_argument("Client certificate is empty");
    }   
    pImpl->mHaveClientCertificate = true;
    pImpl->mClientCertificate = cert;
}

std::optional<std::string> GRPCServerOptions::getClientCertificate() const noexcept
{
    return pImpl->mHaveClientCertificate ? 
           std::make_optional<std::string> (pImpl->mClientCertificate) :
           std::nullopt;
}

/// Access token
void GRPCServerOptions::setAccessToken(const std::string &token)
{
    if (token.empty()){throw std::invalid_argument("Token is empty");}
    pImpl->mHaveAccessToken = true;
    pImpl->mAccessToken = token;
}

std::optional<std::string> GRPCServerOptions::getAccessToken() const noexcept
{
    return pImpl->mHaveAccessToken ?
           std::make_optional<std::string> (pImpl->mAccessToken) : std::nullopt;
}

