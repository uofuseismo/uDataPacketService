#include <cmath>
#include <string>
#include <thread>
#include <random>
#include <chrono>
#include <bit>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <google/protobuf/util/time_util.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketService/server.hpp"
#include "uDataPacketService/serverOptions.hpp"
#include "uDataPacketService/subscriptionManager.hpp"
#include "uDataPacketService/subscriptionManagerOptions.hpp"
#include "uDataPacketService/stream.hpp"
#include "uDataPacketService/streamOptions.hpp"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketService/grpcServerOptions.hpp"
#include "utilities.hpp"

using namespace UDataPacketService;

TEST_CASE("UdataPacketServer", "[ServerOptions]")
{
    const std::string host{"this-compy"};
    const uint16_t port{8424};
    UDataPacketService::GRPCServerOptions grpcOptions;
    grpcOptions.setHost(host);
    grpcOptions.setPort(port);

    ServerOptions options;
    constexpr int maxSubscribers{13};
    options.setGRPCOptions(grpcOptions);
    options.setMaximumNumberOfSubscribers(maxSubscribers);
    REQUIRE(options.getMaximumNumberOfSubscribers() == maxSubscribers);
    REQUIRE(options.getGRPCOptions().getHost() == host);
    REQUIRE(options.getGRPCOptions().getPort() == port);
}
 

TEST_CASE("UDataPacketServer", "[BasicSubscribe]")
{

}

