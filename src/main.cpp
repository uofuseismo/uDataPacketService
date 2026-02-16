import ProgramOptions;

#include <iostream>
#include <csignal>
#include <filesystem>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace
{   
std::atomic<bool> mInterrupted{false};

class Process
{
public:
    Process(const UDataPacketService::ProgramOptions &options,
            std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger)
    {
    }

    /// Handles sigterm and sigint
    static void signalHandler(const int )
    {   
        mInterrupted = true;
    }

    static void catchSignals()
    {   
        struct sigaction action;
        action.sa_handler = signalHandler;
        action.sa_flags = 0;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT,  &action, NULL);
        sigaction(SIGTERM, &action, NULL);
    }   

//private:
    UDataPacketService::ProgramOptions mOptions; 
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
};

}

int main(int argc, char *argv[])
{
    // Get the ini file from the command line
    std::filesystem::path iniFile;
    try
    {
        auto [iniFileName, isHelp] =
            UDataPacketService::parseCommandLineOptions(argc, argv);
        if (isHelp){return EXIT_SUCCESS;}
        iniFile = iniFileName;
    }
    catch (const std::exception &e) 
    {
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed getting command line options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    UDataPacketService::ProgramOptions programOptions;
    try
    {
        programOptions = UDataPacketService::parseIniFile(iniFile);
    }
    catch (const std::exception &e)
    {
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed getting command line options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    if (getenv("OTEL_SERVICE_NAME") == nullptr)
    {   
        constexpr int overwrite{1};
        setenv("OTEL_SERVICE_NAME",
               programOptions.applicationName.c_str(),
               overwrite);
    }   

    return EXIT_SUCCESS;
}
