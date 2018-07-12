#ifdef ENABLE_ONVIF
#include "dw_ptz_controller.h"

#include "digital_watchdog_resource.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

using namespace nx::core;

namespace {
    const QString flipParamId = lit("flipmode1");
    const QString mirrirParamId = lit("mirrormode1");
}

QnDwPtzController::QnDwPtzController(const QnDigitalWatchdogResourcePtr& resource):
    base_type(resource),
    m_resource(resource)
{
    connect(resource.data(), &QnDigitalWatchdogResource::advancedParameterChanged, this,
        &QnDwPtzController::at_physicalParamChanged);
    updateFlipState();
}

void QnDwPtzController::updateFlipState() {
    Qt::Orientations flip = 0;

    const auto flipValue = m_resource->getAdvancedParameter(flipParamId);
    if (flipValue)
    {
        if (flipValue->toInt() == 1)
            flip ^= Qt::Vertical | Qt::Horizontal;
    }
    else
    {
        return;
    }

    const auto mirrorValue = m_resource->getAdvancedParameter(mirrirParamId);
    if (mirrorValue)
    {
        if (mirrorValue->toInt() == 1)
            flip ^= Qt::Horizontal;
    }
    else
    {
        return;
    }

    m_flip = flip;
}

QnDwPtzController::~QnDwPtzController() {
    return;
}

bool QnDwPtzController::continuousMove(
    const nx::core::ptz::Vector& speedVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    auto localSpeed = speedVector;

    if(m_flip & Qt::Horizontal)
        localSpeed.pan = -localSpeed.pan;
    if(m_flip & Qt::Vertical)
        localSpeed.tilt = -localSpeed.tilt;

    return base_type::continuousMove(localSpeed, options);
}

bool QnDwPtzController::getFlip(
    Qt::Orientations *flip,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting flip - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    *flip = m_flip;
    return true;
}

void QnDwPtzController::at_physicalParamChanged(const QString& id, const QString& /*value*/)
{
    // DW cameras doesn't show actual settings if read it immediately.
    if (id == flipParamId || id == mirrirParamId)
        QTimer::singleShot(500, this, SLOT(updateFlipState()));
}

#endif // ENABLE_ONVIF
