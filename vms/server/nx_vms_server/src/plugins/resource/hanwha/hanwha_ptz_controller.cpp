#include "hanwha_ptz_controller.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_common.h"
#include "hanwha_utils.h"
#include "vms_server_hanwha_ini.h"

#include <core/ptz/ptz_preset.h>
#include <utils/common/app_info.h>
#include <api/resource_property_adaptor.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <core/resource_management/resource_data_pool.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const float kNormilizedLimit = 100;
static const int kHanwhaAbsoluteMoveCoefficient = 10000;

} // namespace

using namespace nx::core;
namespace core_ptz = nx::core::ptz;

HanwhaPtzController::HanwhaPtzController(const HanwhaResourcePtr& resource):
    QnBasicPtzController(resource),
    m_hanwhaResource(resource),
    m_presetManager(std::make_unique<HanwhaMappedPresetManager>(resource))
{
}

Ptz::Capabilities HanwhaPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    const auto itr = m_ptzCapabilities.find(options.type);
    if (itr == m_ptzCapabilities.cend())
        return Ptz::NoPtzCapabilities;

    return itr->second;
}

void HanwhaPtzController::setPtzCapabilities(const HanwhaPtzCapabilitiesMap& capabilities)
{
    m_ptzCapabilities = capabilities;
}

void HanwhaPtzController::setPtzLimits(const QnPtzLimits& limits)
{
    m_ptzLimits = limits;
    m_presetManager->setMaxPresetNumber(limits.maxPresetNumber);
}

void HanwhaPtzController::setPtzTraits(const QnPtzAuxiliaryTraitList& traits)
{
    m_ptzTraits = traits;
}

void HanwhaPtzController::setPtzRanges(const HanwhaPtzRangeMap& ranges)
{
    m_ptzRanges = ranges;

    const auto configurationalRangesItr = ranges.find(core_ptz::Type::configurational);
    if (configurationalRangesItr != ranges.cend())
    {
        m_commandStreamer = std::make_unique<HanwhaPtzCommandStreamer>(
            m_hanwhaResource,
            configurationalRangesItr->second);
    }
}

bool HanwhaPtzController::continuousMove(
    const nx::core::ptz::Vector& speedVector,
    const nx::core::ptz::Options& options)
{
    if (m_commandStreamer && options.type == core::ptz::Type::configurational)
    {
        if (!hasAnyCapability(Ptz::ContinuousPtrzCapabilities, core::ptz::Type::configurational))
            return false;

        return m_commandStreamer->continuousMove(speedVector);
    }

    const auto hanwhaSpeed = toHanwhaSpeed(speedVector);

    std::map<QString, QString> params;
    params.emplace(kHanwhaChannelProperty, channel());
    if (speedVector.isNull())
    {
        params.emplace(kHanwhaPanProperty, "0");
        params.emplace(kHanwhaTiltProperty, "0");
        params.emplace(kHanwhaZoomProperty, "0");
        m_lastParamValue.clear();
    }
    else
    {
        auto addIfNeeded = [&](const QString& paramName, float value)
        {
            if (!qFuzzyEquals(value, m_lastParamValue[paramName]))
                params.emplace(paramName, QString::number((int)value));
            m_lastParamValue[paramName] = value;
        };

        if (useNormalizedSpeed())
            params.emplace(kHanwhaNormalizedSpeedProperty, kHanwhaTrue);

        addIfNeeded(kHanwhaPanProperty, hanwhaSpeed.pan);
        addIfNeeded(kHanwhaTiltProperty, hanwhaSpeed.tilt);
        addIfNeeded(kHanwhaZoomProperty, hanwhaSpeed.zoom);
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.control(
        "ptzcontrol/continuous",
        params);

    return response.isSuccessful();
}

bool HanwhaPtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (m_commandStreamer && options.type == core::ptz::Type::configurational)
    {
        if (!hasAnyCapability(Ptz::ContinuousFocusCapability, core::ptz::Type::configurational))
            return false;

        return m_commandStreamer->continuousFocus(speed);
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.control(
        lit("ptzcontrol/continuous"),
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaFocusProperty, toHanwhaFocusCommand(speed)}
        });

    return response.isSuccessful();
}

bool HanwhaPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal /*speed*/,
    const nx::core::ptz::Options& options)
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Absolute movement"));
        return false;
    }

    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto hanwhaPosition = toHanwhaPosition(position);

    const auto response = helper.control(
        "ptzcontrol/absolute",
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPanProperty, QString::number(hanwhaPosition.pan)},
            {kHanwhaTiltProperty, QString::number(hanwhaPosition.tilt)},
            {kHanwhaZoomProperty, QString::number(hanwhaPosition.zoom)}
        });

    return response.isSuccessful();
}

bool HanwhaPtzController::relativeMove(
    const nx::core::ptz::Vector& relativeMovementVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Relative movement"));
        return false;
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto hanwhaRelativeMovement = toHanwhaRelativeMovement(relativeMovementVector);
    if (hanwhaRelativeMovement == std::nullopt)
        return false;

    const auto response = helper.control(
        "ptzcontrol/relative",
        {
            {kHanwhaChannelProperty, channel()},
            {kHanwhaPanProperty, QString::number(hanwhaRelativeMovement->pan, 'f', 2)},
            {kHanwhaTiltProperty, QString::number(hanwhaRelativeMovement->tilt, 'f', 2)},
            {kHanwhaZoomProperty, QString::number(hanwhaRelativeMovement->zoom, 'f', 2)}
        });

    return response.isSuccessful();
}

bool HanwhaPtzController::relativeFocus(
    qreal relativeMovement,
    const nx::core::ptz::Options& options)
{
    NX_WARNING(
        this,
        lm("Relative focus is not implemented for resource %1 (%2)")
            .args(resource()->getName(), resource()->getId()));

    return false;
}

bool HanwhaPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Viewport movement"));
        return false;
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.control(
        lit("ptzcontrol/areazoom"),
        makeViewPortParameters(aspectRatio, viewport));

    return response.isSuccessful();
}

bool HanwhaPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* outPosition,
    const nx::core::ptz::Options& options) const
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Getting current position"));
        return false;
    }

    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
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

    if (x != boost::none)
        outPosition->pan = x.get();

    if (y != boost::none)
        outPosition->tilt = y.get();

    if (z != boost::none)
        outPosition->zoom = z.get();

    return true;
}

bool HanwhaPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Getting limits"));
        return false;
    }

    if (space != Qn::DevicePtzCoordinateSpace)
        return false;

    *limits = m_ptzLimits;
    return true;
}

bool HanwhaPtzController::getFlip(
    Qt::Orientations* flip,
    const nx::core::ptz::Options& options) const
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Getting flip"));
        return false;
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
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

bool HanwhaPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const nx::core::ptz::Options& options) const
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Getting auxiliary traits"));
        return false;
    }

    *auxiliaryTraits = m_ptzTraits;
    return true;
}

bool HanwhaPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, makeWarningMessage("Running auxiliary command"));
        return false;
    }

    if (!hasAnyCapability(Ptz::AuxiliaryPtzCapability, core::ptz::Type::operational))
        return false;

    if (trait.standardTrait() != Ptz::ManualAutoFocusPtzTrait)
        return false;

    const auto focusMode = m_ptzTraits.contains(QnPtzAuxiliaryTrait(kHanwhaSimpleFocusTrait))
        ? lit("SimpleFocus")
        : lit("AutoFocus");

    HanwhaRequestHelper::Parameters parameters{
        {lit("Mode"), focusMode},
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())}
    };

    HanwhaRequestHelper helper(
        m_hanwhaResource->sharedContext(),
        m_hanwhaResource->bypassChannel());

    auto response = helper.control(lit("image/focus"), parameters);
    return response.isSuccessful();
}

QString HanwhaPtzController::channel() const
{
    return QString::number(m_hanwhaResource->getChannel());
}

std::optional<HanwhaRange> HanwhaPtzController::range(
    nx::core::ptz::Type ptzType,
    const HanwhaPtzParameterName& parameterName) const
{
    const auto typeItr = m_ptzRanges.find(ptzType);
    if (typeItr == m_ptzRanges.cend())
        return std::nullopt;

    const auto& ranges = typeItr->second;
    const auto rangeItr = ranges.find(parameterName);
    if (rangeItr != ranges.cend())
        return rangeItr->second;

    return std::nullopt;
}

nx::core::ptz::Vector HanwhaPtzController::toHanwhaSpeed(
    const nx::core::ptz::Vector& speed) const
{
    nx::core::ptz::Vector outSpeed;

    auto toNativeSpeed = [](float maxNegativeSpeed, float maxPositiveSpeed, float normalizedValue)
    {
        if (normalizedValue >= 0.0)
            return normalizedValue * maxPositiveSpeed;
        else
            return normalizedValue * qAbs(maxNegativeSpeed);
    };

    if (useNormalizedSpeed())
    {
        outSpeed.pan = toNativeSpeed(-kNormilizedLimit, kNormilizedLimit, speed.pan);
        outSpeed.tilt = toNativeSpeed(-kNormilizedLimit, kNormilizedLimit, speed.tilt);
        outSpeed.zoom = toNativeSpeed(-kNormilizedLimit, kNormilizedLimit, speed.zoom);
    }
    else
    {
        outSpeed.pan = toNativeSpeed(m_ptzLimits.minPanSpeed, m_ptzLimits.maxPanSpeed, speed.pan);
        outSpeed.tilt = toNativeSpeed(
            m_ptzLimits.minTiltSpeed,
            m_ptzLimits.maxTiltSpeed, speed.tilt);
        outSpeed.zoom = toNativeSpeed(
            m_ptzLimits.minZoomSpeed,
            m_ptzLimits.maxZoomSpeed, speed.zoom);
    }

    return outSpeed;
}

nx::core::ptz::Vector HanwhaPtzController::toHanwhaPosition(
    const nx::core::ptz::Vector& position) const
{
    return position; //< TODO: #dmishin implement
}

std::optional<nx::core::ptz::Vector> HanwhaPtzController::toHanwhaRelativeMovement(
    const nx::core::ptz::Vector& relativeMovement) const
{
    core_ptz::Vector result;
    auto convert =
        [this](const HanwhaPtzParameterName& parameterName, double parameterValue)
            -> std::optional<double>
        {
            const auto parameterRange = range(core_ptz::Type::operational, parameterName);
            if (parameterRange == std::nullopt)
                return std::nullopt;

            const auto value = parameterRange->mapValue(parameterValue);
            if (value == std::nullopt)
                return std::nullopt;

            bool success = false;
            const auto numericValue = value->toDouble(&success);
            if (success)
                return numericValue;

            return std::nullopt;
        };

    struct ParameterToConvert
    {
        double* converted;
        const double& valueToConvert;
        const QString parameterName;
    };

    static const QString kRelativePrefix("Relative.%1");
    const std::vector<ParameterToConvert> parametersToConvert = {
        {&result.pan, relativeMovement.pan, kRelativePrefix.arg(kHanwhaPanProperty)},
        {&result.tilt, relativeMovement.tilt, kRelativePrefix.arg(kHanwhaTiltProperty)},
        {&result.zoom, relativeMovement.zoom, kRelativePrefix.arg(kHanwhaZoomProperty)}
    };

    for (auto parameter: parametersToConvert)
    {
        if (qFuzzyIsNull(parameter.valueToConvert))
            continue;

        const auto convertedValue = convert(parameter.parameterName, parameter.valueToConvert);
        if (convertedValue == std::nullopt)
            return std::nullopt;

        *parameter.converted = *convertedValue;
    }

    return result;
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
    result.emplace(kHanwhaChannelProperty, channel());

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
                qRound(((topLeft.x() + 0.5) * kHanwhaAbsoluteMoveCoefficient)),
                kHanwhaAbsoluteMoveCoefficient));

        y1 = QString::number(
            qBound(
                0,
                qRound(((topLeft.y() + 0.5) * kHanwhaAbsoluteMoveCoefficient)),
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

bool HanwhaPtzController::hasAnyCapability(
    Ptz::Capabilities capabilities,
    nx::core::ptz::Type ptzType) const
{
    const auto itr = m_ptzCapabilities.find(ptzType);
    if (itr == m_ptzCapabilities.cend())
        return false;

    return Ptz::NoPtzCapabilities != (itr->second & capabilities);
}

bool HanwhaPtzController::useNormalizedSpeed() const
{
    auto resData = m_hanwhaResource->resourceData();
    bool normilizedSpeedDisabled = resData.value<bool>(lit("disableNormalizedSpeed"), false);

    return m_ptzTraits.contains(QnPtzAuxiliaryTrait(kHanwhaNormalizedSpeedPtzTrait))
        && ini().allowNormalizedPtzSpeed
        && !normilizedSpeedDisabled;
}

QString HanwhaPtzController::makeWarningMessage(const QString& text) const
{
    static const QString kWarningMessagePattern(
        "%1 - wrong PTZ type. Only operational PTZ is supported. Resource %2 (%3)");
    return kWarningMessagePattern.arg(text, resource()->getName(), resource()->getId().toString());
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
