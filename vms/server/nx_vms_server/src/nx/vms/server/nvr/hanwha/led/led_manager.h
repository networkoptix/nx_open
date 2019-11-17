#pragma once

#include <nx/vms/server/nvr/i_led_manager.h>

namespace nx::vms::server::nvr::hanwha {

class ILedPlatformAbstraction;

class LedManager: public ILedManager
{
public:
    LedManager(std::unique_ptr<ILedPlatformAbstraction> platformAbstraction);

    virtual ~LedManager() override;

    virtual std::vector<LedDescription> ledDescriptions() const override;

    virtual bool setLedState(const QString& ledId, LedState state) override;

private:
    std::unique_ptr<ILedPlatformAbstraction> m_platformAbstraction;
};

} // namespace nx::vms::server::nvr::hanwha
