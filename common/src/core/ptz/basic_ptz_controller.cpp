#include "basic_ptz_controller.h"

QnBasicPtzController::QnBasicPtzController(const QnResourcePtr& resource):
    base_type(resource)
{
}

Ptz::Capabilities QnBasicPtzController::getCapabilities() const
{
    return Ptz::NoPtzCapabilities;
}

bool QnBasicPtzController::continuousMove(const QVector3D& /*spped*/)
{
    return false;
}

bool QnBasicPtzController::continuousFocus(qreal /*speed*/)
{
    return false;
}

bool QnBasicPtzController::absoluteMove(
    Qn::PtzCoordinateSpace /*space*/,
    const QVector3D& /*position*/,
    qreal /*speed*/)
{
    return false;
}

bool QnBasicPtzController::viewportMove(
    qreal /*ratio*/,
    const QRectF& /*viewport*/,
    qreal /*speed*/)
{
    return false;
}

bool QnBasicPtzController::getPosition(
    Qn::PtzCoordinateSpace /*space*/,
    QVector3D* /*position*/) const
{
    return false;
}

bool QnBasicPtzController::getLimits(
    Qn::PtzCoordinateSpace /*space*/,
    QnPtzLimits* /*limits*/) const
{
    return false;
}

bool QnBasicPtzController::getFlip(Qt::Orientations* /*flip*/) const
{
    return false;
}

bool QnBasicPtzController::createPreset(const QnPtzPreset& /*preset*/)
{
    return false;
}

bool QnBasicPtzController::updatePreset(const QnPtzPreset& /*preset*/)
{
    return false;
}

bool QnBasicPtzController::removePreset(const QString& /*presetId*/)
{
    return false;
}

bool QnBasicPtzController::activatePreset(const QString& /*presetId*/, qreal /*speed*/)
{
    return false;
}

bool QnBasicPtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    return false;
}

bool QnBasicPtzController::createTour(const QnPtzTour& /*tour*/)
{
    return false;
}

bool QnBasicPtzController::removeTour(const QString& /*tourId*/)
{
    return false;
}

bool QnBasicPtzController::activateTour(const QString& /*tourId*/)
{
    return false;
}

bool QnBasicPtzController::getTours(QnPtzTourList* /*tours*/) const
{
    return false;
}

bool QnBasicPtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    return false;
}

bool QnBasicPtzController::updateHomeObject(const QnPtzObject& /*object*/)
{
    return false;
}

bool QnBasicPtzController::getHomeObject(QnPtzObject* /*object*/) const
{
    return false;
}

bool QnBasicPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* /*traits*/) const
{
    return false;
}

bool QnBasicPtzController::runAuxilaryCommand(
    const QnPtzAuxilaryTrait& /*trait*/,
    const QString& /*data*/)
{
    return false;
}

