#pragma once

#include <nx/vms/server/nvr/i_led_manager.h>

namespace nx::vms::server::nvr::hanwha {

class LedManager: public ILedManager
{
public:
    virtual std::vector<LedDescriptor> ledDescriptors() const override;

    virtual std::vector<LedState> ledStates() const override;

    virtual bool enable(const QString& ledId) override;

    virtual bool disable(const QString& ledId) override;
};

} // namespace nx::vms::server::nvr::hanwha
