#pragma once

#include <chrono>

#include "proxy_image_provider.h"

namespace nx::vms::client::desktop {

/**
 * This image provider allows loading of underlying im
 */
class DelayingProxyImageProvider: public ProxyImageProvider
{
    Q_OBJECT
    using base_type = ProxyImageProvider;

public:
    using base_type::base_type; //< Forward constructors.

    static std::chrono::milliseconds minimumDelay();
    static void setMinimumDelay(std::chrono::milliseconds value);

    static int maximumQueueLength();
    static void setMaximumQueueLength(int value);

protected:
    virtual void doLoadAsync() override;

private:
    struct Manager;
};

} // namespace nx::vms::client::desktop
