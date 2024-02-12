// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include <nx/reflect/string_conversion.h>
#include <nx/utils/string.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource/camera_resource.h>

namespace nx {
namespace vms {
namespace event {

PluginDiagnosticEvent::PluginDiagnosticEvent(
    qint64 timeStamp,
    const nx::Uuid& engineResourceId,
    const QString& caption,
    const QString& description,
    nx::vms::api::EventLevel level,
    const QnVirtualCameraResourcePtr& device)
    :
    base_type(EventType::pluginDiagnosticEvent, QnResourcePtr(), timeStamp),
    m_engineResourceId(engineResourceId),
    m_caption(caption),
    m_description(description)
{
    m_metadata.level = level;
    if (!device.isNull())
        m_metadata.cameraRefs.push_back(device->getId().toString());
}

bool PluginDiagnosticEvent::checkEventParams(const EventParameters& params) const
{
    const auto ruleLevels =
        nx::reflect::fromString<nx::vms::api::EventLevels>(params.inputPortId.toStdString());

    const auto& ruleCameras = params.metadata.cameraRefs;
    const auto& ruleResource = params.eventResourceId;

    // Check Level flag
    const bool isValidLevel = ruleLevels & m_metadata.level;

    // Check Engine Resource id (null is used for 'Any Resource')
    const bool isValidResource = ruleResource.isNull()
        || m_engineResourceId == ruleResource;

    const bool isValidCamera = ruleCameras.empty()
        || (m_metadata.cameraRefs.size() == 1
            && std::find(
                ruleCameras.begin(),
                ruleCameras.end(),
                m_metadata.cameraRefs.front())
            != ruleCameras.end());

    return isValidLevel && isValidResource && isValidCamera
        && checkForKeywords(m_caption, params.caption)
        && checkForKeywords(m_description, params.description);
}

EventParameters PluginDiagnosticEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.eventResourceId = m_engineResourceId;
    params.caption = m_caption;
    params.description = m_description;
    params.metadata = m_metadata;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
