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

    return options;
}

}
