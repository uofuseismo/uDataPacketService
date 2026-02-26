#ifndef UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_OPTIONS_HPP
#define UDATA_PACKET_SERVICE_SUBSCRIPTION_MANAGER_OPTIONS_HPP
#include <memory>
namespace UDataPacketService
{
 class StreamOptions;
}
namespace UDataPacketService
{
/// @class SubscriptionManagerOptions "subscriptionManagerOptions.hpp"
/// @brief The options defining the behavior of the subscription manager.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class SubscriptionManagerOptions
{
public:
    /// @brief Constructor.
    SubscriptionManagerOptions();
    /// @brief Copy constructor.
    SubscriptionManagerOptions(const SubscriptionManagerOptions &options);
    /// @brief Move constructor.
    SubscriptionManagerOptions(SubscriptionManagerOptions &&options) noexcept;

    /// @brief Sets the maximum number of subscribers.
    /// @param[in] maxSubscribers  The maximum number of subscribers.
    void setMaximumNumberOfSubscribers(int maxSubscribers);
    /// @result The maximum number of subscribers.
    [[nodiscard]] int getMaximumNumberOfSubscribers() const noexcept;

    /// @brief Sets the options defining the behavior of the data streams.
    /// @param[in] options   The data streams options. 
    void setStreamOptions(const StreamOptions &options);
    /// @result The options defining the behavior of the data streams.
    [[nodiscard]] StreamOptions getStreamOptions() const noexcept;

    /// @brief Destructor.
    ~SubscriptionManagerOptions();
    /// @brief Copy assignment.
    SubscriptionManagerOptions& operator=(const SubscriptionManagerOptions &options);
    /// @brief Move assignment.
    SubscriptionManagerOptions& operator=(SubscriptionManagerOptions &&options) noexcept;
private:
    class SubscriptionManagerOptionsImpl;
    std::unique_ptr<SubscriptionManagerOptionsImpl> pImpl;
};
}
#endif
