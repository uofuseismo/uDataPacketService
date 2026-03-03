#include <string>
#include <algorithm>
#include <filesystem>
#include "uDataPacketService/grpcClientOptions.hpp"

using namespace UDataPacketService;

class GRPCClientOptions::GRPCClientOptionsImpl
{
public:
    std::string mHost{"localhost"};
    std::string mAccessToken;
    std::string mServerCertificate;
    std::string mClientCertificate;
    std::string mClientKey;
    uint16_t mPort{50000};
    bool mHaveServerCertificate{false}; 
    bool mHaveClientCertificate{false};
    bool mHaveClientKey{false};
    bool mHaveAccessToken{false};
};

/// Constructor
GRPCClientOptions::GRPCClientOptions() :
    pImpl(std::make_unique<GRPCClientOptionsImpl> ())
{
}

/// Copy constructor
GRPCClientOptions::GRPCClientOptions(const GRPCClientOptions &options)
{
    *this = options;
}

/// Move constructor
GRPCClientOptions::GRPCClientOptions(GRPCClientOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
GRPCClientOptions& 
GRPCClientOptions::operator=(const GRPCClientOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<GRPCClientOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
GRPCClientOptions& 
GRPCClientOptions::operator=(GRPCClientOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
GRPCClientOptions::~GRPCClientOptions() = default; 

/// Host
void GRPCClientOptions::setHost(const std::string &hostIn)
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

std::string GRPCClientOptions::getHost() const noexcept
{
    return pImpl->mHost;
}

std::string UDataPacketService::makeAddress(const GRPCClientOptions &options)
{
    return options.getHost() + ":" + std::to_string(options.getPort());
}

/// Port
void GRPCClientOptions::setPort(const uint16_t port)
{
    if (port < 1)
    {
        throw std::invalid_argument("port must be positive");
    }
    pImpl->mPort = port;
}

uint16_t GRPCClientOptions::getPort() const noexcept
{
    return pImpl->mPort;
}

/// Server cert
void GRPCClientOptions::setServerCertificate(const std::string &cert)
{
    if (cert.empty())
    {
        throw std::invalid_argument("Server certificate is empty");
    }
    pImpl->mHaveServerCertificate = true;
    pImpl->mServerCertificate = cert;
}

std::optional<std::string> GRPCClientOptions::getServerCertificate() const noexcept
{
    return pImpl->mHaveServerCertificate ? 
           std::make_optional<std::string> (pImpl->mServerCertificate) :
           std::nullopt;
}

/// Client cert
void GRPCClientOptions::setClientCertificate(const std::string &cert)
{
    if (cert.empty())
    {   
        throw std::invalid_argument("Client certificate is empty");
    }   
    pImpl->mHaveClientCertificate = true;
    pImpl->mClientCertificate = cert;
}

std::optional<std::string> GRPCClientOptions::getClientCertificate() const noexcept
{
    return pImpl->mHaveClientCertificate ? 
           std::make_optional<std::string> (pImpl->mClientCertificate) :
           std::nullopt;
}

void GRPCClientOptions::setClientKey(const std::string &key)
{   
    if (key.empty())
    {   
        throw std::invalid_argument("Client key is empty");
    }
    pImpl->mHaveClientKey = true;
    pImpl->mClientKey = key;
}

std::optional<std::string> GRPCClientOptions::getClientKey() const noexcept
{
    return pImpl->mHaveClientKey ? 
           std::make_optional<std::string> (pImpl->mClientKey) :
           std::nullopt;
}

/// Access token
void GRPCClientOptions::setAccessToken(const std::string &token)
{
    if (token.empty()){throw std::invalid_argument("Token is empty");}
    pImpl->mHaveAccessToken = true;
    pImpl->mAccessToken = token;
}

std::optional<std::string> GRPCClientOptions::getAccessToken() const noexcept
{
    return pImpl->mHaveAccessToken ?
           std::make_optional<std::string> (pImpl->mAccessToken) : std::nullopt;
}

