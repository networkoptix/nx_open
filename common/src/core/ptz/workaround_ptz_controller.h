#pragma once

#include <core/ptz/proxy_ptz_controller.h>
#include <nx/core/ptz/override.h>
#include <nx/core/ptz/type.h>

class QnWorkaroundPtzController: public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

public:
    QnWorkaroundPtzController(const QnPtzControllerPtr &baseController);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;
    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;

private:
    nx::core::ptz::Override m_override;
};

