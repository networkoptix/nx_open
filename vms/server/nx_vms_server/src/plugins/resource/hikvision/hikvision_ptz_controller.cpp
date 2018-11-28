#include "hikvision_ptz_controller.h"

#include <core/resource/resource.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>

namespace nx {
namespace vms::server {
namespace plugins {
namespace hikvision {

IsapiPtzController::IsapiPtzController(const QnResourcePtr& resource, QAuthenticator authenticator):
    QnBasicPtzController(resource),
    m_client(resource->getUrl(), authenticator),
    m_presetMappings(resource, "isapiPtzPresetMappings")
{
    if (const auto channels = m_client.get(url()))
    {
        if (const auto channel = channels->child("PTZChannel"))
            m_channel = channel->integer("id");
    }

    if (!m_channel)
    {
        NX_DEBUG(this, "Unable to detect channel");
        return;
    }

    if (const auto capabilities = m_client.get(url("capabilities")))
        loadCapabilities(*capabilities);

    NX_DEBUG(this, "Initilizad channel %1, capabilities: %2",
        *m_channel, QnLexical::serialized(m_capabilities));

    if (!m_specialPresetNumbers.empty())
        NX_DEBUG(this, "Special preset numbers: %1", containerString(m_specialPresetNumbers));
}

Ptz::Capabilities IsapiPtzController::getCapabilities(
    const nx::core::ptz::Options& /*options*/) const
{
    QnMutexLocker lock(&m_mutex);
    return m_capabilities;
}

bool IsapiPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    QnMutexLocker lock(&m_mutex);
    if (!isSupported(options))
        return false;

    static const utils::log::Message request(R"xml(
    <PTZData version="2.0" xmlns="http://www.isapi.org/ver20/XMLSchema">
        <pan>%1</pan>
        <tilt>%2</tilt>
        <zoom>%3</zoom>
    </PTZData>
    )xml");

    const auto scaled = speed.scaleSpeed(m_limits);
    const auto result = m_client.put(url("continuous"),
        request.args((int) scaled.pan, (int) scaled.tilt, (int) scaled.zoom));

    NX_DEBUG(this, "Continuous move to %1, scaled %2, result: %3", speed, scaled, result);
    return result;
}

bool IsapiPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal /*speed*/,
    const nx::core::ptz::Options& options)
{
    QnMutexLocker lock(&m_mutex);
    if (!isSupported(options, space))
        return false;

    static const utils::log::Message request(R"xml(
    <PTZData version=“2.0” xmlns=“http://www.isapi.org/ver20/XMLSchema”>
        <AbsoluteHigh>
            <elevation>%1</elevation>
            <azimuth>%2</azimuth>
            <absoluteZoom>%3</absoluteZoom>
        </AbsoluteHigh>
    </PTZData>
    )xml");

    const auto result = m_client.put(url("absolute"),
        request.args((int) position.tilt, (int) position.pan, (int) position.zoom));

    NX_DEBUG(this, "Absolute move to %1, result: %2", position, result);
    return result;
}

bool IsapiPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* position,
    const nx::core::ptz::Options& options) const
{
    QnMutexLocker lock(&m_mutex);
    if (!isSupported(options, space))
        return false;

    const auto response = m_client.get(url("status"));
    if (!response)
        return false;

    const auto absolute = response->child("AbsoluteHigh");
    if (!absolute)
        return false;

    const auto tilt = absolute->integer("elevation");
    const auto pan = absolute->integer("azimuth");
    const auto zoom = absolute->integer("absoluteZoom");
    if (!pan || !tilt || !zoom)
        return false;

    position->pan = *pan;
    position->tilt = *tilt;
    position->zoom = *zoom;
    NX_DEBUG(this, "Current position: %1", position);
    return true;
}

bool IsapiPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    QnMutexLocker lock(&m_mutex);
    if (!isSupported(options, space))
        return false;

    *limits = m_limits;
    return true;
}

bool IsapiPtzController::getPresets(QnPtzPresetList* presets) const
{
    QnMutexLocker lock(&m_mutex);
    const auto cameraPresets = readCameraPresets();
    if (!cameraPresets)
        return false;

    auto presetMappings = m_presetMappings.read();
    bool presetMappingsChanged = false;
    presets->clear();
    for (const auto& [idOnCamera, nameOnCamera]: *cameraPresets)
    {
        auto& preset = presetMappings[idOnCamera];
        if (preset.id.isEmpty())
        {
            preset.id = QnUuid::createUuid().toSimpleString();
            preset.name = nameOnCamera;
            presetMappingsChanged = true;
            NX_DEBUG(this, "Import camera preset %1 -> %2(%3)", idOnCamera, preset.name, preset.id);
        }

        presets->push_back(preset);
    }

    utils::remove_if(
        presetMappings,
        [this, &cameraPresets, &presetMappingsChanged](const auto& id, const auto& mapping)
        {
            if (cameraPresets->count(id))
                return false;

            presetMappingsChanged = true;
            NX_DEBUG(this, "Remove deleted preset %1 -> %2(%3)", id, mapping.name, mapping.id);
            return true;
        });

    if (presetMappingsChanged)
        m_presetMappings.write(presetMappings);

    NX_VERBOSE(this, "Found %1 presets on camera", presets->size());
    return true;
}

bool IsapiPtzController::activatePreset(const QString& presetId, qreal /*speed*/)
{
    QnMutexLocker lock(&m_mutex);
    auto presetMappings = m_presetMappings.read();
    const auto seceltedMapping = utils::find_if(
        presetMappings,
        [&](const auto& mapping) { return mapping.second.id == presetId; });

    if (!seceltedMapping)
    {
        NX_WARNING(this, "Unable to find preset %1 for activation", presetId);
        return false;
    }

    const auto result = m_client.put(url(lm("presets/%1/goto").arg(seceltedMapping->first)));
    NX_DEBUG(this, "Activate preset %1 -> %2 (%3), result: %4",
        seceltedMapping->first, seceltedMapping->second.name, presetId, result);

    return result;
}

bool IsapiPtzController::createPreset(const QnPtzPreset& preset)
{
    QnMutexLocker lock(&m_mutex);
    const auto existingPresets = readCameraPresets();
    if (!existingPresets)
        return false;

    int idOnCamera = 1;
    while (existingPresets->count(idOnCamera) || m_specialPresetNumbers.count(idOnCamera))
        ++idOnCamera;

    static const utils::log::Message request(R"xml(
    <PTZPreset version="2.0" xmlns="http://www.isapi.org/ver20/XMLSchema">
        <enabled>true</enabled>
        <id>%1</id>
        %2
    </PTZPreset>
    )xml");

    QString nameTag;
    if (m_isPresetNameSupported)
        nameTag = lm("<presetName>VMS %1</presetName>").args(idOnCamera);

    const auto result = m_client.put(url(lm("presets/%1").arg(idOnCamera)),
        request.args(idOnCamera, nameTag));

    if (result)
        m_presetMappings.edit()->emplace(idOnCamera, preset);

    NX_DEBUG(this, "New preset %1 -> %2 (%3), result: %4",
        idOnCamera, preset.name, preset.id, result);

    return result;
}

bool IsapiPtzController::updatePreset(const QnPtzPreset& preset)
{
    QnMutexLocker lock(&m_mutex);
    auto presetMappings = m_presetMappings.read();
    const auto seceltedMapping = utils::find_if(
        presetMappings,
        [&](const auto& mapping) { return mapping.second.id == preset.id; });

    if (!seceltedMapping)
    {
        NX_WARNING(this, "Unable to find preset %1 for rename to %2", preset.id, preset.name);
        return false;
    }

    seceltedMapping->second.name = preset.name;
    m_presetMappings.write(presetMappings);
    NX_DEBUG(this, "Renamed preset %1 -> %2 (%3)", seceltedMapping->first, preset.name, preset.id);
    return true;
}

bool IsapiPtzController::removePreset(const QString& presetId)
{
    QnMutexLocker lock(&m_mutex);
    auto presetMappings = m_presetMappings.read();
    const auto seceltedMapping = std::find_if(
        presetMappings.begin(), presetMappings.end(),
        [&](const auto& mapping) { return mapping.second.id == presetId; });

    if (seceltedMapping == presetMappings.end())
    {
        NX_WARNING(this, "Unable to find preset %1 for remove", presetId);
        return false;
    }

    const auto result = m_client.delete_(url(lm("presets/%1").arg(seceltedMapping->first)));
    NX_DEBUG(this, "Remove preset %1 -> %2 (%3), result: %4",
        seceltedMapping->first, seceltedMapping->second.name, presetId, result);

    if (result)
    {
        presetMappings.erase(seceltedMapping);
        m_presetMappings.write(presetMappings);
    }

    return result;
}

QString IsapiPtzController::url(const QString& action) const
{
    if (action.isNull())
        return "ISAPI/PTZCtrl/channels";

    if (m_channel)
        return lm("ISAPI/PTZCtrl/channels/%1/%2").args(*m_channel, action);

    NX_ASSERT(false, "Channel is not detected yet");
    return "";
}

bool IsapiPtzController::isSupported(
    const nx::core::ptz::Options& options, Qn::PtzCoordinateSpace space) const
{
    if (options.type != nx::core::ptz::Type::operational)
    {
        NX_WARNING(this, "Only operational PTZ is supported");
        return false;
    }

    if (space != Qn::DevicePtzCoordinateSpace)
    {
        NX_WARNING(this, "Only device coordinate PTZ space is supported");
        return false;
    }

    return true;
}

void IsapiPtzController::loadCapabilities(
    const nx::plugins::utils::XmlRequestHelper::Result& capabilities)
{
    if (const auto space = capabilities.child("AbsolutePanTiltPositionSpace"))
    {
        m_capabilities |= Ptz::AbsolutePanTiltCapabilities;
        if (const auto range = space->child("XRange"))
        {
            m_limits.maxPan = range->integerOrZero("Max");
            m_limits.minPan = range->integerOrZero("Min");
        }
        if (const auto range = space->child("YRange"))
        {
            m_limits.maxTilt = range->integerOrZero("Max");
            m_limits.minTilt = range->integerOrZero("Min");
        }
    }
    if (const auto space = capabilities.child("AbsoluteZoomPositionSpace"))
    {
        m_capabilities |= Ptz::AbsoluteZoomCapability;
        if (const auto range = space->child("ZRange"))
        {
            m_capabilities |= Ptz::AbsoluteZoomCapability;
            m_limits.maxFov = range->integerOrZero("Max");
            m_limits.minFov = range->integerOrZero("Min");
        }
    }
    if (const auto space = capabilities.child("ContinuousPanTiltSpace"))
    {
        m_capabilities |= Ptz::ContinuousPanTiltCapabilities;
        if (const auto range = space->child("XRange"))
        {
            // Channel <panMaxSpeed> should be used instead.
            m_limits.maxPanSpeed = range->integerOrZero("Max");
            m_limits.minPanSpeed = range->integerOrZero("Min");
        }
        if (const auto range = space->child("YRange"))
        {
            // Channel <tiltMaxSpeed> should be used instead.
            m_limits.maxTiltSpeed = range->integerOrZero("Max");
            m_limits.minTiltSpeed = range->integerOrZero("Min");
        }
    }
    if (const auto space = capabilities.child("ContinuousZoomSpace"))
    {
        m_capabilities |= Ptz::ContinuousZoomCapability;
        if (const auto range = space->child("ZRange"))
        {
            // TODO: Find out if it's relevant.
            m_limits.maxZoomSpeed = range->integerOrZero("Max");
            m_limits.minZoomSpeed = range->integerOrZero("Min");
        }
    }
    if (const auto presetNumber = capabilities.integer("maxPresetNum"))
    {
        m_capabilities |= Ptz::PresetsPtzCapability;
        m_maxPresetNumber = *presetNumber;
        if (const auto name = capabilities.child("PresetNameCap"))
        {
            m_isPresetNameSupported = name->booleanOrFalse("presetNameSupport");
            if (const auto numbers = name->child("specialNo"))
            {
                for (const auto& number: numbers->attributeOrEmpty("opt").split(','))
                    m_specialPresetNumbers.insert(number.toInt());
            }
        }
    }
}

std::optional<std::map<int, QString>> IsapiPtzController::readCameraPresets() const
{
    const auto response = m_client.get(url("presets"));
    if (!response)
        return std::nullopt;

    std::map<int, QString> presets;
    for (const auto& record: response->children("PTZPreset"))
    {
        const auto id = record.integer("id");
        if (!id)
            return std::nullopt;

        if (m_specialPresetNumbers.count(*id))
            continue; // These presets are unable to be activated or changed.

        presets.emplace(*id, record.stringOrEmpty("presetName"));
    }

    return presets;
}

} // namespace hikvision
} // namespace plugins
} // namespace vms::server
} // namespace nx
