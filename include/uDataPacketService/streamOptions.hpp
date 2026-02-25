#ifndef UDATA_PACKET_SERVICE_STREAM_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_STREAM_OPTIONS_HPP
#include <memory>
namespace UDataPacketService
{
/// @class StreamOptions "streamOptions.hpp"
/// @brief Defines the options for a stream.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class StreamOptions
{
public:
    /// @brief Default constructor.
    StreamOptions();
    /// @brief Copy constructor.
    StreamOptions(const StreamOptions &options);
    /// @brief Move cnostructor.
    StreamOptions(StreamOptions &&options) noexcept;

    /// @brief Every stream makes a small queue for the benefit of the 
    ///        subscriber (a process running on a different thread).
    ///        This allows the reader thread a little time to get the
    ///        latest data packet before the writer writes the next
    ///        packet. 
    /// @param[in] queueSize  This must be positive.
    void setMaximumQueueSize(const int queueSize);
    /// @result The maximum queue size. 
    /// @note By default this is 8.
    [[nodiscard]] int getMaximumQueueSize() const noexcept;

    /// @brief Destructor.
    ~StreamOptions();
    /// @brief Copy assignment.
    StreamOptions& operator=(const StreamOptions &);
    /// @brief Move assignment.
    StreamOptions& operator=(StreamOptions &&) noexcept;
private:
    class StreamOptionsImpl;
    std::unique_ptr<StreamOptionsImpl> pImpl;
};

}
#endif
