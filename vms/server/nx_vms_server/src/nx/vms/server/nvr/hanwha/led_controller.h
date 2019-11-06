#pragma once

#include <nx/vms/server/nvr/i_led_controller.h>

namespace nx::vms::server::nvr::hanwha {

class LedController: public ILedController
{
public:
    virtual void start() override;

    virtual std::vector<LedDescriptor> ledDescriptors() const override;

    virtual std::map<LedId, LedState> ledStates() const override;

    virtual bool setState(const LedId& ledId, LedState state) override;

};

} // namespace nx::vms::server::nvr::hanwha
