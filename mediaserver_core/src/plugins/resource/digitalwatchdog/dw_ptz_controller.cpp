#ifdef ENABLE_ONVIF
#include "dw_ptz_controller.h"

#include "digital_watchdog_resource.h"

namespace {
    const QString flipParamId = lit("flipmode1");
    const QString mirrirParamId = lit("mirrormode1");
}

QnDwPtzController::QnDwPtzController(const QnDigitalWatchdogResourcePtr &resource):
    base_type(resource),
    m_resource(resource)
{
    connect(resource.data(), &QnDigitalWatchdogResource::advancedParameterChanged, this, &QnDwPtzController::at_physicalParamChanged);
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

bool QnDwPtzController::continuousMove(const QVector3D &speed) {
    QVector3D localSpeed = speed;

    if(m_flip & Qt::Horizontal)
        localSpeed.setX(-localSpeed.x());
    if(m_flip & Qt::Vertical)
        localSpeed.setY(-localSpeed.y());

    return base_type::continuousMove(localSpeed);
}

bool QnDwPtzController::getFlip(Qt::Orientations *flip) const
{
    *flip = m_flip;
    return true;
}

void QnDwPtzController::at_physicalParamChanged(const QString& id, const QString& value) {
    Q_UNUSED(value);
    if (id == flipParamId || id == mirrirParamId)
        QTimer::singleShot(500, this, SLOT(updateFlipState())); // DW cameras doesn't show actual settings if read it immediately
}


#endif // ENABLE_ONVIF
