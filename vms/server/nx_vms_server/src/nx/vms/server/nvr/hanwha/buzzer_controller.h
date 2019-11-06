#pragma once

#include <nx/vms/server/nvr/i_buzzer_controller.h>

namespace nx::vms::server::nvr::hanwha {

class BuzzerController: public IBuzzerController
{
public:
    virtual void start() override;

    virtual bool enable(std::chrono::milliseconds duration) override;

    virtual bool disable() override;
};

} // namespace nx::vms::server::nvr::hanwha
