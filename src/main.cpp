import ProgramOptions;
import Logger;
import Utilities;
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
        assert(mLogger != nullptr);
        assert(mSubscriber != nullptr);
#endif
        mKeepRunning.store(true);
        mFutures.push_back(std::async(&Process::propagateImportPackets, this));
        mFutures.push_back(mSubscriber->start());
        handleMainThread();
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

    /// Sends the import packets to the client(s)
    void propagateImportPackets()
    {
        const std::chrono::microseconds timeOut{10};
        while (mKeepRunning.load())
        {
            UDataPacketServiceAPI::V1::Packet packet;
            if (mImportQueue.try_pop(packet))
            {

            }
            else
            {
                std::this_thread::sleep_for(timeOut); 
            }
        }
    }

    // Print some summary statistics
    void printSummary()
    {
        if (mOptions.printSummaryInterval.count() <= 0){return;}
        auto now
            = std::chrono::duration_cast<std::chrono::microseconds>
              ((std::chrono::high_resolution_clock::now()).time_since_epoch());
        if (now > mLastPrintSummary + mOptions.printSummaryInterval)
        {
            mLastPrintSummary = now;

        }
    }

    /// Check futures
    [[nodiscard]]
    bool checkFuturesOkay(const std::chrono::milliseconds &timeOut)
    {
        bool isOkay{true};
        for (auto &future : mFutures)
        {
            try
            {
                auto status = future.wait_for(timeOut);
                if (status == std::future_status::ready)
                {
                    future.get();
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_CRITICAL(mLogger,
                                       "Fatal error detected from thread: {}",
                                       std::string {e.what()});
                isOkay = false;
            }
        }
        return isOkay;
    }

    // Let main thread handle signals from OS and deal with exceptions
    void handleMainThread()
    {
        SPDLOG_LOGGER_DEBUG(mLogger, "Main thread entering waiting loop");
        catchSignals();
        while (!mStopRequested)
        {
            if (mInterrupted)
            {   
                SPDLOG_LOGGER_INFO(mLogger,
                                   "SIGINT/SIGTERM signal received!");
                mStopRequested = true;
                mShutdownRequested = true;
                mShutdownCondition.notify_all();
                break;
            }
            if (!checkFuturesOkay(std::chrono::milliseconds {5}))
            {
                SPDLOG_LOGGER_CRITICAL(
                   mLogger,
                   "Futures exception caught; terminating app");
                mStopRequested = true;
                mShutdownRequested = true;
                mShutdownCondition.notify_all();
                break;
            }
            printSummary();
            std::unique_lock<std::mutex> lock(mStopMutex);
            mStopCondition.wait_for(lock,
                                    std::chrono::milliseconds {100},
                                    [this]
                                    {
                                          return mStopRequested;
                                    });
            lock.unlock();
        }
        if (mStopRequested)
        {
            SPDLOG_LOGGER_DEBUG(mLogger, "Stop request received.  Exiting...");
            stop();
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
    mutable std::mutex mStopMutex;
    mutable std::mutex mShutdownMutex;
    std::condition_variable mStopCondition;
    std::condition_variable mShutdownCondition;
    std::chrono::microseconds mLastPrintSummary
    {
        UDataPacketService::Utilities::getNow<std::chrono::microseconds> ()
    };
    int mMaximumImportQueueSize{8192};
    std::atomic<bool> mKeepRunning{true};
    bool mStopRequested{false};
    bool mShutdownRequested{false};
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
