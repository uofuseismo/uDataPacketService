import ProgramOptions;
import Logger;
import PacketConverter;

#include <iostream>
#include <csignal>
#include <filesystem>
#include <atomic>
#include <condition_variable>
#include <mutex>
#ifndef NDEBUG
#include <cassert>
#endif
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <absl/log/initialize.h>
#include <tbb/concurrent_queue.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uDataPacketImportAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketService/subscriber.hpp"

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
#ifndef NDEBUG
        assert(mLogger != nullptr);
#endif
        mSubscriber
            = std::make_unique<UDataPacketService::Subscriber>
              (mOptions.subscriberOptions, mAddPacketCallbackFunction, mLogger);
        mMaximumImportQueueSize = mOptions.maximumImportQueueSize;
        mImportQueue.set_capacity(mMaximumImportQueueSize);
    }

    ~Process()
    {
        stop();
    }

    void stop()
    {
        mKeepRunning.store(false);
        if (mSubscriber){mSubscriber->stop();}
        for (auto &future : mFutures)
        {   
            if (future.valid()){future.get();}
        }   
    }

    void start()
    {
#ifndef NDEBUG
        assert(mSubscriber != nullptr);
#endif
        mKeepRunning.store(true);
        mFutures.push_back(mSubscriber->start());
    }

    void addPacketCallback(UDataPacketImportAPI::V1::Packet &&inputPacket)
    {
        try
        {
            auto newPacket
                = UDataPacketService::convert(std::move(inputPacket));
            while (mImportQueue.size() >= mMaximumImportQueueSize)
            {   
                UDataPacketServiceAPI::V1::Packet workSpace;
                if (!mImportQueue.try_pop(workSpace))
                {   
                    SPDLOG_LOGGER_WARN(mLogger, 
                        "Failed to pop front of queue while adding packet");
                    break;
                }   
            }   
            // Send the packet
            if (!mImportQueue.try_push(std::move(newPacket)))
            {   
                SPDLOG_LOGGER_WARN(mLogger,
                    "Failed to add packet to import queue");
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Failed to add packet because {}",
                               std::string {e.what()});
        }
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
    std::unique_ptr<UDataPacketService::Subscriber> mSubscriber{nullptr};
    std::vector<std::future<void>> mFutures;
    tbb::concurrent_bounded_queue<UDataPacketServiceAPI::V1::Packet>
        mImportQueue;
    std::function<void(UDataPacketImportAPI::V1::Packet &&)>
        mAddPacketCallbackFunction
    {
        std::bind(&::Process::addPacketCallback, this,
                  std::placeholders::_1)
    };  
    int mMaximumImportQueueSize{8192};
    std::atomic<bool> mKeepRunning{true};
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

    auto logger
        = UDataPacketService::Logger::initialize(programOptions);
    // Initialize the metrics singleton
    //USEEDLinkToDataPacketImportProxy::Metrics::initializeMetricsSingleton();

    try 
    {   
        if (programOptions.exportMetrics)
        {
            SPDLOG_LOGGER_INFO(logger, "Initializing metrics");
            //UDataPacketService::Metrics::initialize(programOptions);
        }
    }   
    catch (const std::exception &e) 
    {   
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to initialize metrics because {}",
                               std::string {e.what()});
        if (programOptions.exportLogs)
        {
            UDataPacketService::Logger::cleanup();
        }
        return EXIT_FAILURE;
    }   


    //absl::InitializeLog();
    try
    {
        ::Process process(programOptions, logger);
        process.start();
        if (programOptions.exportMetrics)
        {
            //UDataPacketService::Metrics::cleanup();
        }
        if (programOptions.exportLogs)
        {
            UDataPacketService::Logger::cleanup();
        }
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger, "Main process failed with {}",
                               std::string {e.what()});
        if (programOptions.exportMetrics)
        {
            //UDataPacketService::Metrics::cleanup();
        }
        if (programOptions.exportLogs)
        {
            UDataPacketService::Logger::cleanup();
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
