#pragma once

#include <nx/vms/server/nvr/i_buzzer_manager.h>

namespace nx::vms::server::nvr::hanwha {

class BuzzerManager: public IBuzzerManager
{
public:
    virtual bool enable(std::chrono::milliseconds duration) override;

    virtual bool disable() override;
};

} // namespace nx::vms::server::nvr::hanwha
