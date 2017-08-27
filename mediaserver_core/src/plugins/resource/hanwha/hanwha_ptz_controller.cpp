#if defined(ENABLE_HANWHA)

#include "hanwha_ptz_controller.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaPtzController::HanwhaPtzController(const QnResourcePtr& resource):
    QnBasicPtzController(resource)
{

}

Ptz::Capabilities HanwhaPtzController::getCapabilities() const
{
    return 0;
}

bool HanwhaPtzController::continuousMove(const QVector3D& speed)
{
    return false;
}

bool HanwhaPtzController::continuousFocus(qreal speed)
{
    return false;
}

bool HanwhaPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D& position, qreal speed)
{
    return false;
}

bool HanwhaPtzController::viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed)
{
    return false;
}

bool HanwhaPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const
{
    return false;
}

bool HanwhaPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const
{
    return false;
}

bool HanwhaPtzController::getFlip(Qt::Orientations* flip) const
{
    return false;
}

bool HanwhaPtzController::createPreset(const QnPtzPreset& preset)
{
    return false;
}

bool HanwhaPtzController::updatePreset(const QnPtzPreset& preset)
{
    return false;
}

bool HanwhaPtzController::removePreset(const QString& presetId)
{
    return false;
}

bool HanwhaPtzController::activatePreset(const QString& presetId, qreal speed)
{
    return false;
}

bool HanwhaPtzController::getPresets(QnPtzPresetList* presets) const
{
    return false;
}

bool HanwhaPtzController::createTour(const QnPtzTour& tour)
{
    return false;
}

bool HanwhaPtzController::removeTour(const QString& tourId)
{
    return false;
}

bool HanwhaPtzController::activateTour(const QString& tourId)
{
    return false;
}

bool HanwhaPtzController::getTours(QnPtzTourList* tours) const
{
    return false;
}

bool HanwhaPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    return false;
}

bool HanwhaPtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    return false;
}

bool HanwhaPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    return false;
}

bool HanwhaPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const
{
    return false;
}

bool HanwhaPtzController::runAuxilaryCommand(const QnPtzAuxilaryTrait& trait, const QString& data)
{
    return false;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)