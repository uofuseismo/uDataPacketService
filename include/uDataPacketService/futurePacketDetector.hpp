#ifndef UDATA_PACKET_SERVICE_FUTURE_PACKET_DETECTOR_HPP
#define UDATA_PACKET_SERVICE_FUTURE_PACKET_DETECTOR_HPP
#include <chrono>
#include <string>
#include <memory>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}
namespace UDataPacketService
{

class FuturePacketDetectorOptions
{
public:
    /// @brief Constructor.
    FuturePacketDetectorOptions();
    /// @brief Copy constructor.
    FuturePacketDetectorOptions(const FuturePacketDetectorOptions &options);
    /// @brief Move constructor.
    FuturePacketDetectorOptions(FuturePacketDetectorOptions &&options) noexcept;

    /// @brief Sets the max amount of time into the future from which data
    ///        can arrive.
    /// @param[in] maxFutureTime  Data will be considered valid only if
    ///                           it's last sample is less than
    ///                           now + maxFutureTime.
    void setMaxFutureTime(const std::chrono::microseconds &maxExpiredTime);
    /// @result If any sample in the packet has a time that exceeds the current
    ///         time plus getMaxFutureTime() then the packet is rejected.
    /// @note By default this is 0 which is pretty generous considering that
    ///       data generated at the sensor needs to make it back to a data
    ///       cneter.
    [[nodiscard]] std::chrono::microseconds getMaxFutureTime() const noexcept;
   
    /// @brief Destructor.
    ~FuturePacketDetectorOptions();

    /// @brief Copy assignment.
    FuturePacketDetectorOptions& operator=(const FuturePacketDetectorOptions &options);
    /// @brief Move constructor.
    FuturePacketDetectorOptions& operator=(FuturePacketDetectorOptions &&options) noexcept;
private:
    class FuturePacketDetectorOptionsImpl;
    std::unique_ptr<FuturePacketDetectorOptionsImpl> pImpl;
};
}

namespace UDataPacketService
{
/// @class FuturePacketDetector futurePacketDetector.hpp
/// @brief Tests whether or not a packet contains data from the future.  This
///        indicates that there is a timing error.
/// @copyright Ben Baker (University of Utah) distributed under the MIT NO AI
///            license.
class FuturePacketDetector
{
public:
    /// @brief Constructs the future data detector.
    explicit FuturePacketDetector(const FuturePacketDetectorOptions &options);
    /// @brief Copy constructor.
    FuturePacketDetector(const FuturePacketDetector &detector);
    /// @brief Move constructor.
    FuturePacketDetector(FuturePacketDetector &&detector) noexcept;

    /// @param[in] packet  The protobuf representation of a data packet.
    /// @result True indicates the data packet does not appear to have any future data.
    [[nodiscard]] bool allow(const UDataPacketServiceAPI::V1::Packet &packet) const;

    /// @result True indicates the data packet does not appear to have any future data.
    [[nodiscard]] bool operator()(const UDataPacketServiceAPI::V1::Packet &packet) const;

    /// @brief Destructor.
    ~FuturePacketDetector();
    /// @brief Copy assignment.
    FuturePacketDetector& operator=(const FuturePacketDetector &detector);
    /// @brief Move constructor.
    FuturePacketDetector& operator=(FuturePacketDetector &&detector) noexcept;

    FuturePacketDetector() = delete;
private:
    class FuturePacketDetectorImpl;
    std::unique_ptr<FuturePacketDetectorImpl> pImpl;
};
}
#endif
