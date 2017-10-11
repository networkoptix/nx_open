#if defined(ENABLE_HANWHA)

#include "hanwha_ptz_controller.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_common.h"
#include "hanwha_utils.h"

#include <core/ptz/ptz_preset.h>
#include <utils/common/app_info.h>
#include <api/resource_property_adaptor.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const float kNormilizedLimit = 100;
static const int kHanwhaAbsoluteMoveCoefficient = 10000;

} // namespace

HanwhaPtzController::HanwhaPtzController(const HanwhaResourcePtr& resource):
    QnBasicPtzController(resource),
    m_hanwhaResource(resource),
    m_presetManager(std::make_unique<HanwhaMappedPresetManager>(resource))
{
}

Ptz::Capabilities HanwhaPtzController::getCapabilities() const
{
    return m_ptzCapabilities;
}

void HanwhaPtzController::setPtzCapabilities(Ptz::Capabilities capabilities)
{
    m_ptzCapabilities = capabilities;
}

void HanwhaPtzController::setPtzLimits(const QnPtzLimits& limits)
{
    m_ptzLimits = limits;
    m_presetManager->setMaxPresetNumber(limits.maxPresetNumber);
}

void HanwhaPtzController::setPtzTraits(const QnPtzAuxilaryTraitList& traits)
{
    m_ptzTraits = traits;
}

bool HanwhaPtzController::continuousMove(const QVector3D& speed)
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto hanwhaSpeed = toHanwhaSpeed(speed);

    std::map<QString, QString> params;
    auto addIfNeed = [&](const QString& paramName, float value)
    {
        if (!qFuzzyEquals(value, m_lastParamValue[paramName]))
            params.emplace(paramName, QString::number((int)value));
        m_lastParamValue[paramName] = value;
    };

    params.emplace(kHanwhaChannelProperty, channel());
    if (m_ptzTraits.contains(QnPtzAuxilaryTrait(HanwhaResource::kNormalizedSpeedPtzTrait)))
        params.emplace(kHanwhaNormalizedSpeedProperty, kHanwhaTrue);

    addIfNeed(kHanwhaPanProperty, hanwhaSpeed.x());
    addIfNeed(kHanwhaTiltProperty, hanwhaSpeed.y());
    addIfNeed(kHanwhaZoomProperty, hanwhaSpeed.z());

    const auto response = helper.control(
        lit("ptzcontrol/continuous"),
        params);

    return response.isSuccessful();
}

bool HanwhaPtzController::continuousFocus(qreal speed)
{
    HanwhaRequestHelper helper(m_hanwhaResource);

    const auto response = helper.control(
        lit("ptzcontrol/continuous"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaFocusProperty, toHanwhaFocusCommand(speed)}
        });

    return response.isSuccessful();
}

bool HanwhaPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D& position, qreal /*speed*/)
{
    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto hanwhaPosition = toHanwhaPosition(position);

    const auto response = helper.control(
        lit("ptzcontrol/absolute"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPanProperty, QString::number(hanwhaPosition.x())},
            {kHanwhaTiltProperty, QString::number(hanwhaPosition.y())},
            {kHanwhaZoomProperty, QString::number(hanwhaPosition.z())}
        });

    return response.isSuccessful();
}

bool HanwhaPtzController::viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed)
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    helper.control(
        lit("ptzcontrol/areazoom"),
        makeViewPortParameters(aspectRatio, viewport));

    return false;
}

bool HanwhaPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const
{
    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource);
    helper.setIgnoreMutexAnalyzer(true);

    const auto response = helper.view(
        lit("ptzcontrol/query"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPtzQueryProperty, lit("%1,%2,%3") //< TODO: #dmishin check if it is valid
                .arg(kHanwhaPanProperty)
                .arg(kHanwhaTiltProperty)
                .arg(kHanwhaZoomProperty)}
        });

    if (!response.isSuccessful())
        return false;

    const auto parameters = response.response();

    const auto x = response.parameter<double>(kHanwhaPanProperty);
    const auto y = response.parameter<double>(kHanwhaTiltProperty);
    const auto z = response.parameter<double>(kHanwhaZoomProperty);
    
    if (x.is_initialized())
        position->setX(x.get());

    if (y.is_initialized())
        position->setY(y.get());

    if (z.is_initialized())
        position->setZ(z.get());

    return true;
}

bool HanwhaPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const
{
    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    *limits = m_ptzLimits;
    return true;
}

bool HanwhaPtzController::getFlip(Qt::Orientations* flip) const
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.view(lit("image/flip"));

    if (!response.isSuccessful())
        return false;

    *flip = 0;

    const auto horizontalFlip = response.parameter<bool>(
        channelParameter(m_hanwhaResource->getChannel(), kHanwhaHorizontalFlipProperty));

    const auto verticalFlip = response.parameter<bool>(
        channelParameter(m_hanwhaResource->getChannel(), kHanwhaVerticalFlipProperty));

    if (horizontalFlip.is_initialized() && *horizontalFlip)
        *flip |= Qt::Orientation::Horizontal;

    else if (verticalFlip.is_initialized() && *verticalFlip)
        *flip |= Qt::Orientation::Vertical;

    return true;
}

bool HanwhaPtzController::createPreset(const QnPtzPreset& preset)
{
    return m_presetManager->createPreset(preset);
}

bool HanwhaPtzController::updatePreset(const QnPtzPreset& preset)
{
    return m_presetManager->updatePreset(preset);
}

bool HanwhaPtzController::removePreset(const QString& presetId)
{
    return m_presetManager->removePreset(presetId);
}

bool HanwhaPtzController::activatePreset(const QString& presetId, qreal speed)
{
   return m_presetManager->activatePreset(presetId, speed);
}

bool HanwhaPtzController::getPresets(QnPtzPresetList* presets) const
{
    return m_presetManager->presets(presets);
}

bool HanwhaPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const
{
    *auxilaryTraits = m_ptzTraits;
    return true;
}

bool HanwhaPtzController::runAuxilaryCommand(const QnPtzAuxilaryTrait& trait, const QString& data)
{
    if (!m_ptzCapabilities.testFlag(Ptz::AuxilaryPtzCapability))
        return false;

    if (trait.standardTrait() == Ptz::ManualAutoFocusPtzTrait)
    {
        HanwhaRequestHelper helper(m_hanwhaResource);
        auto response = helper.control(
            lit("image/focus"),
            {{lit("Mode"), lit("AutoFocus")}});

        return response.isSuccessful();
    }
    
    return false;
}

QString HanwhaPtzController::channel() const
{
    return QString::number(m_hanwhaResource->getChannel());
}

QVector3D HanwhaPtzController::toHanwhaSpeed(const QVector3D& speed) const
{
    QVector3D outSpeed;

    auto toNativeSpeed = [](float maxNegativeSpeed, float maxPositiveSpeed, float normalizedValue)
    {
        if (normalizedValue >= 0.0)
            return normalizedValue * maxPositiveSpeed;
        else
            return normalizedValue * qAbs(maxNegativeSpeed);
    };

    if (m_ptzTraits.contains(QnPtzAuxilaryTrait(HanwhaResource::kNormalizedSpeedPtzTrait)))
    {
        outSpeed.setX(toNativeSpeed(-kNormilizedLimit, kNormilizedLimit, speed.x()));
        outSpeed.setY(toNativeSpeed(-kNormilizedLimit, kNormilizedLimit, speed.y()));
        outSpeed.setZ(toNativeSpeed(-kNormilizedLimit, kNormilizedLimit, speed.z()));
    }
    else
    {
        outSpeed.setX(toNativeSpeed(m_ptzLimits.minPanSpeed, m_ptzLimits.maxPanSpeed, speed.x()));
        outSpeed.setY(toNativeSpeed(m_ptzLimits.minTiltSpeed, m_ptzLimits.maxTiltSpeed, speed.y()));
        outSpeed.setZ(toNativeSpeed(m_ptzLimits.minZoomSpeed, m_ptzLimits.maxZoomSpeed, speed.z()));
    }

    return outSpeed;
}

QVector3D HanwhaPtzController::toHanwhaPosition(const QVector3D& position) const
{
    return position; //< TODO: #dmishin implement
}

QString HanwhaPtzController::toHanwhaFocusCommand(qreal speed) const
{
    if (qFuzzyIsNull(speed))
        return kHanwhaStopFocusMove;

    if (speed > 0)
        return kHanwhaFarFocusMove;

    return kHanwhaNearFocusMove;
}

std::map<QString, QString> HanwhaPtzController::makeViewPortParameters(
    qreal aspectRatio,
    const QRectF rect) const
{
    std::map<QString, QString> result;

    if (rect.width() > 1 || rect.height() > 1)
    {
        result.emplace(lit("Type"), lit("1x"));
        return result;
    }
    
    result.emplace(lit("Type"), lit("ZoomIn"));

    auto topLeft = rect.topLeft();
    auto bottomRight = rect.bottomRight();

    QString x1;
    QString y1;
    QString x2;
    QString y2;

    bool toPoint = qFuzzyIsNull(rect.width() - 1.0)
        && qFuzzyIsNull(rect.height() - 1.0);

    if (toPoint)
    {
        x1 = QString::number(
            qBound(
                0,
                (int)std::round(((topLeft.x() + 0.5) * kHanwhaAbsoluteMoveCoefficient)),
                kHanwhaAbsoluteMoveCoefficient));

        y1 = QString::number(
            qBound(
                0,
                (int)std::round(((topLeft.y() + 0.5) * kHanwhaAbsoluteMoveCoefficient)),
                kHanwhaAbsoluteMoveCoefficient));

        x2 = x1;
        y2 = y1;
    }
    else
    {
        x1 = QString::number(
            qBound(
                0,
                (int)(topLeft.x() * kHanwhaAbsoluteMoveCoefficient),
                kHanwhaAbsoluteMoveCoefficient));

        y1 = QString::number(
            qBound(
                0,
                (int)(topLeft.y() * kHanwhaAbsoluteMoveCoefficient),
                kHanwhaAbsoluteMoveCoefficient));

        x2 = QString::number(
            qBound(
                0,
                (int)(bottomRight.x() * kHanwhaAbsoluteMoveCoefficient),
                kHanwhaAbsoluteMoveCoefficient));

        y2 = QString::number(
            qBound(
                0,
                (int)(bottomRight.y() * kHanwhaAbsoluteMoveCoefficient),
                kHanwhaAbsoluteMoveCoefficient));
    }

    result.emplace(lit("X1"), x1);
    result.emplace(lit("Y1"), y1);
    result.emplace(lit("X2"), x2);
    result.emplace(lit("Y2"), y2);

    return result;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
