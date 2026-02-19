#include <iostream>
#include <string>
#include <chrono>
#include <mutex>
#include <set>
#include "uDataPacketService/futurePacketDetector.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"

import Utilities;

using namespace UDataPacketService;

class FuturePacketDetectorOptions::FuturePacketDetectorOptionsImpl
{
public:
    std::chrono::microseconds mMaxFutureTime{0};
};

/// Constructor
FuturePacketDetectorOptions::FuturePacketDetectorOptions() :
    pImpl(std::make_unique<FuturePacketDetectorOptionsImpl> ())
{
}

/// Copy constructor
FuturePacketDetectorOptions::FuturePacketDetectorOptions(
    const FuturePacketDetectorOptions &options)
{
    *this = options;
}

/// Copy assignment
FuturePacketDetectorOptions&
FuturePacketDetectorOptions::operator=(
    const FuturePacketDetectorOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<FuturePacketDetectorOptionsImpl> (*options.pImpl);
    return *this;
}
 
/// Move assignment
FuturePacketDetectorOptions&
FuturePacketDetectorOptions::operator=(
    FuturePacketDetectorOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Max future time
void FuturePacketDetectorOptions::setMaxFutureTime(
    const std::chrono::microseconds &duration)
{
    //if (duration.count() < 0)
    //{
    //    spdlog::warn("Future time is negative");
    //} 
    pImpl->mMaxFutureTime = duration;
}

std::chrono::microseconds
    FuturePacketDetectorOptions::getMaxFutureTime() const noexcept
{
    return pImpl->mMaxFutureTime;
}

/// Destructor
FuturePacketDetectorOptions::~FuturePacketDetectorOptions() = default;

///--------------------------------------------------------------------------///

class FuturePacketDetector::FuturePacketDetectorImpl
{
public:
    explicit FuturePacketDetectorImpl
    (
        const FuturePacketDetectorOptions &options
    ) :
        mOptions(options),
        mMaxFutureTime(options.getMaxFutureTime())
    {
    }
    /// Checks the packet
    template<typename U>
    [[nodiscard]] bool allow(const U &packet)
    {
        auto packetEndTime
            = Utilities::getEndTimeInMicroSeconds(packet); // Throws
        // Computing the current time after the scraping the ring is
        // conservative.  Basically, when the max future time is zero,
        // this allows for a zero-latency, 1 sample packet, to be
        // successfully passed through.
        auto nowMuSeconds = Utilities::getNow<std::chrono::microseconds> ();
        auto latestTime = nowMuSeconds + mMaxFutureTime;
        // Packet contains data after max allowable time?
        return (packetEndTime <= latestTime) ? true : false;
    }
//private:
    FuturePacketDetectorOptions mOptions;
    std::set<std::string> mFutureChannels;
    std::chrono::microseconds mMaxFutureTime{0};
};

/// Constructor with options
FuturePacketDetector::FuturePacketDetector
    (
        const FuturePacketDetectorOptions &options
    ) :
    pImpl(std::make_unique<FuturePacketDetectorImpl> (options))
{
}

/// Copy constructor
FuturePacketDetector::FuturePacketDetector(
    const FuturePacketDetector &testFutureDataPacket)
{
    *this = testFutureDataPacket;
}

/// Move constructor
FuturePacketDetector::FuturePacketDetector(
    FuturePacketDetector &&testFutureDataPacket) noexcept
{
    *this = std::move(testFutureDataPacket);
}

/// Copy assignment
FuturePacketDetector& 
FuturePacketDetector::operator=(const FuturePacketDetector &detector)
{
    if (&detector == this){return *this;}
    pImpl = std::make_unique<FuturePacketDetectorImpl> (*detector.pImpl);
    return *this;
}

/// Move assignment
FuturePacketDetector&
FuturePacketDetector::operator=(
    FuturePacketDetector &&detector) noexcept
{
    if (&detector == this){return *this;}
    pImpl = std::move(detector.pImpl);
    return *this;
}

/// Destructor
FuturePacketDetector::~FuturePacketDetector() = default;

/// Allow future packet?
bool FuturePacketDetector::allow(
    const UDataPacketServiceAPI::V1::Packet &packet) const
{
    return pImpl->allow(packet);
}

bool FuturePacketDetector::operator()(
    const UDataPacketServiceAPI::V1::Packet &packet) const
{
    return allow(packet);
}

