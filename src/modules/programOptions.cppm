module;

#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "uDataPacketService/subscriberOptions.hpp"
#include "uDataPacketService/grpcOptions.hpp"

export module ProgramOptions;

namespace UDataPacketService
{

#define APPLICATION_NAME "uDataPacketService"

export struct OTelHTTPMetricsOptions
{
    std::string url{"localhost:4318"};
    std::chrono::milliseconds exportInterval{5000};
    std::chrono::milliseconds exportTimeOut{500};
    std::string suffix{"/v1/metrics"};
};

export struct OTelHTTPLogOptions
{
    std::string url{"localhost:4318"};
    std::filesystem::path certificatePath;
    std::string suffix{"/v1/logs"};
};

export struct ProgramOptions
{
    std::string applicationName{APPLICATION_NAME};
    SubscriberOptions subscriberOptions;
    OTelHTTPMetricsOptions otelHTTPMetricsOptions;
    OTelHTTPLogOptions otelHTTPLogOptions;
    std::chrono::seconds printSummaryInterval{3600};
    int verbosity{3};
    int maximumImportQueueSize{8192};
    bool exportLogs{false};
    bool exportMetrics{false};
};

export
std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[])
{
    std::string iniFile;
    boost::program_options::options_description desc(R"""(
The uDataPacketService provides subscribers access to real-time data packets.

Example usage is:

    uDataPacketService --ini=service.ini

Allowed options)""");
    desc.add_options()
        ("help", "Produces this help message")
        ("ini",  boost::program_options::value<std::string> (),
                 "The initialization file for this executable");
    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);
    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return {iniFile, true};
    }
    if (vm.count("ini"))
    {
        iniFile = vm["ini"].as<std::string>();
        if (!std::filesystem::exists(iniFile))
        {
            throw std::runtime_error("Initialization file: " + iniFile
                                   + " does not exist");
        }
    }
    else
    {
        throw std::runtime_error("Initialization file not specified");
    }
    return {iniFile, false};
}

[[nodiscard]]
std::string getOTelCollectorURL(boost::property_tree::ptree &propertyTree,
                                const std::string &section)
{
    std::string result;
    std::string otelCollectorHost
        = propertyTree.get<std::string> (section + ".host", "");
    uint16_t otelCollectorPort
        = propertyTree.get<uint16_t> (section + ".port", 4218);
    if (!otelCollectorHost.empty())
    {
        result = otelCollectorHost + ":"
               + std::to_string(otelCollectorPort);
    }
    return result;
}

[[nodiscard]] std::string
loadStringFromFile(const std::filesystem::path &path)
{
    std::string result;
    if (!std::filesystem::exists(path)){return result;}
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open " + path.string());
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    file.close();
    result = sstr.str();
    return result;
}

[[nodiscard]] UDataPacketService::GRPCOptions getGRPCOptions(
    const boost::property_tree::ptree &propertyTree,
    const std::string &section)
{
    UDataPacketService::GRPCOptions options;

    auto host
        = propertyTree.get<std::string> (section + ".host",
                                         options.getHost());
    if (host.empty())
    {   
        throw std::runtime_error(section + ".host is empty");
    }   
    options.setHost(host);

    uint16_t port{50000};
    options.setPort(port);

    port = propertyTree.get<uint16_t> (section + ".port", options.getPort());
    options.setPort(port); 

    auto serverCertificate
        = propertyTree.get<std::string> (section + ".serverCertificate", "");
    if (!serverCertificate.empty())
    {   
        if (!std::filesystem::exists(serverCertificate))
        {
            throw std::invalid_argument("gRPC server certificate file "
                                      + serverCertificate
                                      + " does not exist");
        }
        options.setServerCertificate(loadStringFromFile(serverCertificate));
    }

    auto accessToken
        = propertyTree.get_optional<std::string> (section + ".accessToken");
    if (accessToken)
    {
        if (options.getServerCertificate() == std::nullopt)
        {
            throw std::invalid_argument(
                "Must set server certificate to use access token");
        }
        options.setAccessToken(*accessToken);
    }

    auto clientKey
        = propertyTree.get<std::string> (section + ".clientKey", "");
    auto clientCertificate
        = propertyTree.get<std::string> (section + ".clientCertificate", "");
    if (!clientKey.empty() && !clientCertificate.empty())
    {
        if (!std::filesystem::exists(clientKey))
        {
            throw std::invalid_argument("gRPC client key file "
                                      + clientKey
                                      + " does not exist");
        }
        if (!std::filesystem::exists(clientCertificate))
        {
            throw std::invalid_argument("gRPC client certificate file "
                                      + clientCertificate
                                      + " does not exist");
        }
        options.setClientKey(loadStringFromFile(clientKey));
        options.setClientCertificate(loadStringFromFile(clientCertificate));
    }
    return options;
}

export ProgramOptions
    parseIniFile(const std::filesystem::path &iniFile)
{
    ProgramOptions options;
    if (!std::filesystem::exists(iniFile)){return options;}
    // Parse the initialization file
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);
    // Application name
    options.applicationName
        = propertyTree.get<std::string> ("General.applicationName",
                                         options.applicationName);
    if (options.applicationName.empty())
    {
        options.applicationName = APPLICATION_NAME;
    }
    options.verbosity
        = propertyTree.get<int> ("General.verbosity", options.verbosity);
    options.exportMetrics = false;
    options.exportLogs = false;

    // Logging
    OTelHTTPLogOptions logOptions;
    logOptions.url
         = getOTelCollectorURL(propertyTree, "OTelHTTPLogOptions");
    logOptions.suffix
         = propertyTree.get<std::string>
           ("OTelHTTPLogOptions.suffix", "/v1/logs");
    if (!logOptions.url.empty())
    {   
        if (!logOptions.suffix.empty())
        {
            if (!logOptions.url.ends_with("/") &&
                !logOptions.suffix.starts_with("/"))
            {
                logOptions.suffix = "/" + logOptions.suffix;
            }
        }
    }   
    if (!logOptions.url.empty())
    {   
        options.exportLogs = true;
        options.otelHTTPLogOptions = logOptions;
    }

    // Metrics
    OTelHTTPMetricsOptions metricsOptions;
    metricsOptions.url
         = getOTelCollectorURL(propertyTree, "OTelHTTPMetricsOptions");
    metricsOptions.suffix
         = propertyTree.get<std::string> ("OTelHTTPMetricsOptions.suffix",
                                          "/v1/metrics");
    if (!metricsOptions.url.empty())
    {
        if (!metricsOptions.suffix.empty())
        {
            if (!metricsOptions.url.ends_with("/") &&
                !metricsOptions.suffix.starts_with("/"))
            {
                metricsOptions.suffix = "/" + metricsOptions.suffix;
            }
        }
    }

    // Subscriber
    SubscriberOptions subscriberOptions;
    auto subscriberGRPCOptions = getGRPCOptions(propertyTree, "Subscriber");
    subscriberOptions.setGRPCOptions(subscriberGRPCOptions);
    options.subscriberOptions = subscriberOptions;
    
    

    return options;
}

}
