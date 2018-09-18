#include "hikvision_ptz_controller.h"

#include <core/resource/resource.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {
namespace hikvision {

// TODO: Test it.
// Current implementation is according to Hikvision docs but is newer tested.

IsapiPtzController::IsapiPtzController(const QnResourcePtr& resource, QAuthenticator authenticator):
    QnBasicPtzController(resource),
    m_client(resource->getUrl(), authenticator)
{
    const auto response = m_client.get(url());
    if (!response)
        return;

    const auto channels = response->child(lit("PTZChannelList"));
    if (!channels)
        return;

    const auto firstChannel = channels->child(lit("PTZChannel"));
    if (!firstChannel)
        return;

    m_channel = firstChannel->integer(lit("id"));
    if (!m_channel)
        return;

    if (firstChannel->booleanOrFalse(lit("panSupport")))
        m_capabilities |= Ptz::ContinuousPanCapability | Ptz::AbsolutePanCapability;

    if (firstChannel->booleanOrFalse(lit("tiltSupport")))
        m_capabilities |= Ptz::ContinuousTiltCapability | Ptz::AbsoluteTiltCapability;

    if (firstChannel->booleanOrFalse(lit("zoomSupport")))
        m_capabilities |= Ptz::ContinuousZoomCapability | Ptz::AbsoluteZoomCapability;
}

Ptz::Capabilities IsapiPtzController::getCapabilities(
    const nx::core::ptz::Options& /*options*/) const
{
    return m_capabilities;
}

bool IsapiPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    if (!isSupported(options))
        return false;

    static const utils::log::Message request(R"xml(
    <PTZData version="2.0" xmlns="http://www.isapi.org/ver20/XMLSchema">
        <pan>%1</pan>
        <tilt>%2</tilt>
        <zoom>%3</zoom>
    </PTZData>
    )xml");

    return m_client.put(url("continuous"), request.args(speed.pan, speed.tilt, speed.zoom));
}

bool IsapiPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal /*speed*/,
    const nx::core::ptz::Options& options)
{
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

    return m_client.put(url("absolute"), request.args(position.pan, position.tilt, position.zoom));
}

bool IsapiPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* position,
    const nx::core::ptz::Options& options) const
{
    if (!isSupported(options, space))
        return false;

    const auto response = m_client.get(url(lit("status")));
    if (!response)
        return false;

    const auto absolute = response->child(lit("AbsoluteHigh"));
    if (!absolute)
        return false;

    const auto pan = absolute->integer(lit("elevation"));
    const auto tilt = absolute->integer(lit("azimuth"));
    const auto zoom = absolute->integer(lit("absoluteZoom"));
    if (!pan || !tilt || !zoom)
        return false;

    position->pan = *pan;
    position->tilt = *tilt;
    position->zoom = *zoom;
    return true;
}

bool IsapiPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    if (!isSupported(options, space))
        return false;

    limits->maxPan = 2700;
    limits->minPan = -900;
    limits->maxTilt = 2700;
    limits->minTilt = -900;
    limits->minFov = 2700;
    limits->minFov = -900;
    return true;
}

bool IsapiPtzController::getPresets(QnPtzPresetList* presets) const
{
    const auto response = m_client.get(url(lit("presets")));
    if (!response)
        return false;

    for (const auto& preset: response->children(lit("PTZPreset")))
    {
        const auto id = preset.string(lit("id"));
        const auto name = preset.string(lit("presetName"));
        if (!id || !name)
            return false;

        presets->push_back({*id, *name});
    }

    return true;
}

bool IsapiPtzController::activatePreset(const QString& presetId, qreal /*speed*/)
{
    return m_client.put(url(lit("presets/%1/goto").arg(presetId)));
}

bool IsapiPtzController::createPreset(const QnPtzPreset& preset)
{
    return updatePreset(preset);
}

bool IsapiPtzController::updatePreset(const QnPtzPreset& preset)
{
    static const utils::log::Message request(R"xml(
    <PTZPreset version=“2.0” xmlns=“http://www.isapi.org/ver20/XMLSchema”>
        <enabled>false</enabled>
        <id>%1</id>
        <presetName>%2</presetName>
    </PTZPreset>
    )xml");

    return m_client.put(url(lit("presets/%1").arg(preset.id)), request.args(preset.id, preset.name));
}

bool IsapiPtzController::removePreset(const QString& presetId)
{
    return m_client.delete_(url(lit("presets/%1").arg(presetId)));
}

QString IsapiPtzController::url(const QString& action) const
{
    if (action.isNull())
        return lit("ISAPI/PTZCtrl/channels");

    if (m_channel)
        return lit("ISAPI/PTZCtrl/channels/%1/%2").arg(*m_channel).arg(action);

    NX_WARNING(this, "Channel is not detected yet");
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

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
