#if defined(ENABLE_HANWHA)

#include "hanwha_ptz_controller.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_common.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const float kLowerPtzLimit = -100;
static const float kHigherPtzLimit = 100;
static const int kPtzCoefficient = 100;

} // namespace

HanwhaPtzController::HanwhaPtzController(const HanwhaResourcePtr& resource):
    QnBasicPtzController(resource),
    m_hanwhaResource(resource)
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
}

bool HanwhaPtzController::continuousMove(const QVector3D& speed)
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto hanwhaSpeed = toHanwhaSpeed(speed);

    const auto response = helper.control(
        lit("ptzcontrol/continuous"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPanProperty, QString::number((int)hanwhaSpeed.x())},
            {kHanwhaTiltProperty, QString::number((int)hanwhaSpeed.y())},
            {kHanwhaZoomProperty, QString::number((int)hanwhaSpeed.z())},
            {kHanwhaNormalizedSpeedProperty, kHanwhaTrue}
        });

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
    // TODO: #dmishin implement.
    return false;
}

bool HanwhaPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const
{
    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource);

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
    *limits = m_ptzLimits;
    return true;
}

bool HanwhaPtzController::getFlip(Qt::Orientations* flip) const
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.view(lit("image/flip"));

    if (!response.isSuccessful())
        return false;

    flip = 0;

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
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto presetNumber = freePresetNumber();
    if (presetNumber.isEmpty())
        return false;

    const auto response = helper.add(
        lit("ptzconfig/preset"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPresetNumberProperty, presetNumber},
            {kHanwhaPresetNameProperty, preset.name}
        });

    if (!response.isSuccessful())
        return false;

    m_presetNumberById[preset.id] = presetNumber;

    return response.isSuccessful();
}

bool HanwhaPtzController::updatePreset(const QnPtzPreset& preset)
{
    const auto presetNumber = presetNumberFromId(preset.id);
    if (presetNumber.isEmpty())
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.update(
        lit("ptzconfig/preset"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPresetNumberProperty, presetNumber},
            {kHanwhaPresetNameProperty, preset.name}
        });

    return response.isSuccessful();
}

bool HanwhaPtzController::removePreset(const QString& presetId)
{
    const auto presetNumber = presetNumberFromId(presetId);
    if (presetNumber.isEmpty())
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.remove(
        lit("ptzconfig/preset"),
        {{kHanwhaChannelProperty, channel()}});

    return response.isSuccessful();
}

bool HanwhaPtzController::activatePreset(const QString& presetId, qreal /*speed*/)
{
    HanwhaRequestHelper helper(m_hanwhaResource);

    const auto response = helper.control(
        lit("ptzcontrol/preset"),
        {{kHanwhaPresetNumberProperty, presetNumberFromId(presetId)}});

    return response.isSuccessful();
}

bool HanwhaPtzController::getPresets(QnPtzPresetList* presets) const
{
    HanwhaRequestHelper helper(m_hanwhaResource);

    const auto response = helper.view(
        lit("ptzcontrol/preset"),
        {{kHanwhaChannelProperty, channel()}});

    response.isSuccessful();

    for (const auto& preset: response.response())
    {
        const auto split = preset.first.split(L'.');
        if (split.size() != 5)
            continue;

        const auto& presetNumber = split[4];
        const auto& presetName = preset.second;

        QnPtzPreset preset(presetNumber, presetName);
        presets->push_back(preset);
        m_presetNumberById[presetNumber] = presetNumber;
    }

    return true;
}

QString HanwhaPtzController::channel() const
{
    return QString::number(m_hanwhaResource->getChannel());
}

QString HanwhaPtzController::freePresetNumber() const
{
    if (m_ptzLimits.maxPresetNumber == 0)
        return QString();

    QnPtzPresetList presets;
    if (!getPresets(&presets))
        return QString();
 
    std::set<int> availablePresets;
    for (const auto& preset: presets)
    {
        bool success = false;
        const int presetNumber = preset.id.toInt(&success);
        if (!success)
            continue;

        availablePresets.insert(presetNumber);
    }

    const int limit = m_ptzLimits.maxPresetNumber > 0
        ? m_ptzLimits.maxPresetNumber
        : kHanwhaMaxPresetNumber;

    for (auto i = 0; i < limit; ++i)
    {
        const auto itr = availablePresets.find(i);
        if (itr == availablePresets.cend())
            return QString::number(i);
    }

    return QString();
}

QVector3D HanwhaPtzController::toHanwhaSpeed(const QVector3D& speed) const
{
    QVector3D outSpeed;
    outSpeed.setX(qBound(kLowerPtzLimit, speed.x() * kPtzCoefficient, kHigherPtzLimit));
    outSpeed.setY(qBound(kLowerPtzLimit, speed.y() * kPtzCoefficient, kHigherPtzLimit));
    outSpeed.setZ(qBound(kLowerPtzLimit, speed.z() * kPtzCoefficient, kHigherPtzLimit));

    return outSpeed;
}

QVector3D HanwhaPtzController::toHanwhaPosition(const QVector3D& position) const
{
    return position; //< TODO: #dmishin implement
}

QString HanwhaPtzController::toHanwhaFocusCommand(qreal speed) const
{
    if (speed > 0)
        return kHanwhaFarFocusMove;

    if (speed < 0)
        return kHanwhaNearFocusMove;

    return kHanwhaStopFocusMove;
}

QString HanwhaPtzController::presetNumberFromId(const QString& presetId) const
{
    auto itr = m_presetNumberById.find(presetId);
    if (itr == m_presetNumberById.cend())
        return QString();

    return itr->second;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
