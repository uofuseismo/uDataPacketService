#include <cmath>
#include <string>
#include <chrono>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketService/grpcClientOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"

TEST_CASE("UDataPacketService", "[grpcClientOptions]")
{
    SECTION("Defaults")
    {
        UDataPacketService::GRPCClientOptions options;
        REQUIRE(options.getHost() == "localhost");
        REQUIRE(options.getPort() == 50000);
        REQUIRE(options.getAccessToken() == std::nullopt);
        REQUIRE(options.getServerCertificate() == std::nullopt);
        REQUIRE(options.getClientCertificate() == std::nullopt);
        REQUIRE(options.getClientKey() == std::nullopt);
    }

    SECTION("Options")
    {
        std::string host{"some.host.org"};
        std::string token{"super-secret-token"};
        std::string serverCertificate{"some-wonky-hash"};
        std::string clientCertificate{"some-other-hash"};
        std::string clientKey{"some-private-hash"};
        uint16_t port{12345};
        UDataPacketService::GRPCClientOptions options;

        options.setHost(host);
        options.setPort(port);
        options.setServerCertificate(serverCertificate);
        options.setAccessToken(token);
        options.setClientCertificate(clientCertificate);
        options.setClientKey(clientKey);

        REQUIRE(options.getHost() == host);
        REQUIRE(options.getPort() == port);
        REQUIRE(*options.getServerCertificate() == serverCertificate);
        REQUIRE(*options.getAccessToken() == token);
        REQUIRE(*options.getClientCertificate() == clientCertificate);
        REQUIRE(*options.getClientKey() == clientKey);
    }
}

TEST_CASE("UDataPacketService", "[grpcServerOptions]")
{
    SECTION("Defaults")
    {   
        UDataPacketService::GRPCServerOptions options;
        REQUIRE(options.getHost() == "localhost");
        REQUIRE(options.getPort() == 50000);
        REQUIRE(options.getAccessToken() == std::nullopt);
        REQUIRE(options.getServerCertificate() == std::nullopt);
        REQUIRE(options.getServerKey() == std::nullopt);
        REQUIRE(options.getClientCertificate() == std::nullopt);
    }   

    SECTION("Options")
    {   
        std::string host{"some.host.org"};
        std::string token{"super-secret-token"};
        std::string serverCertificate{"some-wonky-hash"};
        std::string serverKey{"some-private-hash"};
        std::string clientCertificate{"some-other-hash"};
        uint16_t port{12345};
        UDataPacketService::GRPCServerOptions options;

        options.setHost(host);
        options.setPort(port);
        options.setServerCertificate(serverCertificate);
        options.setServerKey(serverKey);
        options.setAccessToken(token);
        options.setClientCertificate(clientCertificate);

        REQUIRE(options.getHost() == host);
        REQUIRE(options.getPort() == port);
        REQUIRE(*options.getServerCertificate() == serverCertificate);
        REQUIRE(*options.getServerKey() == serverKey);
        REQUIRE(*options.getAccessToken() == token);
        REQUIRE(*options.getClientCertificate() == clientCertificate);
    }   

}

