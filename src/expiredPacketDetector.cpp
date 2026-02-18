#include <iostream>
#include <string>
#include <chrono>
#include <mutex>
#include <set>
#include <spdlog/spdlog.h>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketService/expiredPacketDetector.hpp"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

import Utilities;

using namespace UDataPacketService;

class ExpiredPacketDetectorOptions::ExpiredPacketDetectorOptionsImpl
{
public:
    std::chrono::microseconds mMaxExpiredTime{std::chrono::minutes {5}};
    std::chrono::seconds mLogBadDataInterval{3600};
};

/// Constructor
ExpiredPacketDetectorOptions::ExpiredPacketDetectorOptions() :
    pImpl(std::make_unique<ExpiredPacketDetectorOptionsImpl> ())
{
}

/// Copy constructor
ExpiredPacketDetectorOptions::ExpiredPacketDetectorOptions(
    const ExpiredPacketDetectorOptions &options)
{
    *this = options;
}

/// Copy assignment
ExpiredPacketDetectorOptions&
ExpiredPacketDetectorOptions::operator=(
    const ExpiredPacketDetectorOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<ExpiredPacketDetectorOptionsImpl> (*options.pImpl);
    return *this;
}
 
/// Move assignment
ExpiredPacketDetectorOptions&
ExpiredPacketDetectorOptions::operator=(
    ExpiredPacketDetectorOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Max expired time
void ExpiredPacketDetectorOptions::setMaxExpiredTime(
    const std::chrono::microseconds &duration)
{
    if (duration.count() <= 0)
    {   
        throw std::invalid_argument("Expired time must be positive");
    }   
    pImpl->mMaxExpiredTime = duration;
}

std::chrono::microseconds
    ExpiredPacketDetectorOptions::getMaxExpiredTime() const noexcept
{
    return pImpl->mMaxExpiredTime;
}

/// Logging interval
void ExpiredPacketDetectorOptions::setLogBadDataInterval(
    const std::chrono::seconds &interval) noexcept
{
    pImpl->mLogBadDataInterval = interval;
    if (interval.count() < 0)
    {   
        pImpl->mLogBadDataInterval = std::chrono::seconds {-1};
    }   
}

std::chrono::seconds
    ExpiredPacketDetectorOptions::getLogBadDataInterval() const noexcept
{
    return pImpl->mLogBadDataInterval;
}

/// Destructor
ExpiredPacketDetectorOptions::~ExpiredPacketDetectorOptions() = default;

///--------------------------------------------------------------------------///

class ExpiredPacketDetector::ExpiredPacketDetectorImpl
{
public:
    ExpiredPacketDetectorImpl(const ExpiredPacketDetectorOptions &options) :
        mOptions(options),
        mMaxExpiredTime(options.getMaxExpiredTime())
    {
        if (mMaxExpiredTime.count() <= 0)
        {
            std::invalid_argument("Max expired time must be positive");
        }
    }
    /// Checks the packet
    template<typename U>
    [[nodiscard]] bool allow(const U &packet)
    {
        auto startTimeMuSec
            = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
                 packet.start_time());
        std::chrono::microseconds packetStartTime{startTimeMuSec};
        auto nowMuSeconds = Utilities::getNow<std::chrono::microseconds> ();
        auto earliestTime = nowMuSeconds - mMaxExpiredTime;
        // Packet contains data before the earliest allowable time
        bool allow = (packetStartTime >= earliestTime) ? true : false;
        return allow;
    }
//private:
    ExpiredPacketDetectorOptions mOptions;
    std::chrono::microseconds mMaxExpiredTime{std::chrono::minutes {5}};
};

/// Constructor with options
ExpiredPacketDetector::ExpiredPacketDetector(
    const ExpiredPacketDetectorOptions &options) :
    pImpl(std::make_unique<ExpiredPacketDetectorImpl> (options))
{
}

/// Copy constructor
ExpiredPacketDetector::ExpiredPacketDetector(
    const ExpiredPacketDetector &testExpiredDataPacket)
{
    *this = testExpiredDataPacket;
}

/// Move constructor
ExpiredPacketDetector::ExpiredPacketDetector(
    ExpiredPacketDetector &&testExpiredDataPacket) noexcept
{
    *this = std::move(testExpiredDataPacket);
}

/// Copy assignment
ExpiredPacketDetector& 
ExpiredPacketDetector::operator=(const ExpiredPacketDetector &detector)
{
    if (&detector == this){return *this;}
    pImpl = std::make_unique<ExpiredPacketDetectorImpl> (*detector.pImpl);
    return *this;
}

/// Move assignment
ExpiredPacketDetector&
ExpiredPacketDetector::operator=(
    ExpiredPacketDetector &&detector) noexcept
{
    if (&detector == this){return *this;}
    pImpl = std::move(detector.pImpl);
    return *this;
}

/// Destructor
ExpiredPacketDetector::~ExpiredPacketDetector() = default;

/// Allow expired packet?

bool ExpiredPacketDetector::allow(
    const UDataPacketServiceAPI::V1::Packet &packet) const
{
    return pImpl->allow(packet);
}

bool ExpiredPacketDetector::operator()(
    const UDataPacketServiceAPI::V1::Packet &packet) const
{
    return allow(packet);
}
