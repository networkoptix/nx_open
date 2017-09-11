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
static const int kHanwhaAbsoluteMoveCoefficient = 10000;

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

void HanwhaPtzController::setPtzTraits(const QnPtzAuxilaryTraitList& traits)
{
    m_ptzTraits = traits;
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
    helper.setAllowLocks(true);

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

#if 0
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

    for (const auto& presetEntry: response.response())
    {
        const auto split = presetEntry.first.split(L'.');
        if (split.size() != 5)
            continue;

        const auto& presetNumber = split[4];
        const auto& presetName = presetEntry.second;

        QnPtzPreset preset(presetNumber, presetName);
        presets->push_back(preset);
        m_presetNumberById[presetNumber] = presetNumber;
    }

    return true;
}
#endif

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
        x1 = QString::number(qBound(0, (int)std::round(((topLeft.x() + 0.5) * 10000)), 10000));
        y1 = QString::number(qBound(0, (int)std::round(((topLeft.y() + 0.5) * 10000)), 10000));
        x2 = QString::number(qBound(0, (int)std::round(((topLeft.x() + 0.5) * 10000)), 10000));
        y2 = QString::number(qBound(0, (int)std::round(((topLeft.y() + 0.5) * 10000)), 10000));
    }
    else
    {
        x1 = QString::number(qBound(0, (int)(topLeft.x() * 10000), 10000));
        y1 = QString::number(qBound(0, (int)(topLeft.y() * 10000), 10000));
        x2 = QString::number(qBound(0, (int)(bottomRight.x() * 10000), 10000));
        y2 = QString::number(qBound(0, (int)(bottomRight.y() * 10000), 10000));
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
