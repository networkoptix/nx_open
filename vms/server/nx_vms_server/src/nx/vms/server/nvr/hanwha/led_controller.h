#pragma once

#include <nx/vms/server/nvr/i_led_controller.h>

namespace nx::vms::server::nvr::hanwha {

class LedController: public ILedController
{
public:
    virtual std::vector<LedDescriptor> ledDescriptors() const override;

    virtual std::map<LedId, LedState> ledStates() const override;

    virtual bool enable(const QString& ledId) override;

    virtual bool disable(const QString& ledId) override;
};

} // namespace nx::vms::server::nvr::hanwha
