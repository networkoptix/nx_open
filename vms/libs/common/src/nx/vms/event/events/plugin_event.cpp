#include "plugin_event.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource/security_cam_resource.h>

namespace nx {
namespace vms {
namespace event {

PluginEvent::PluginEvent(
    qint64 timeStamp,
    const AnalyticsEngineResourcePtr& pluginInstance,
    const QString& caption,
    const QString& description,
    nx::vms::api::EventLevel level,
    const QnSecurityCamResourcePtr& camera)
    :
    base_type(EventType::pluginEvent, QnResourcePtr(), timeStamp),
    m_resourceId(pluginInstance->getId()),
    m_caption(caption),
    m_description(description)
{
    m_metadata.level = level;
    if (!camera.isNull())
        m_metadata.cameraRefs.push_back(camera->getId().toString());
}

bool PluginEvent::checkEventParams(const EventParameters& params) const
{
    const auto ruleLevels =
        QnLexical::deserialized<nx::vms::api::EventLevels>(params.inputPortId);

    const auto& ruleCameras = params.metadata.cameraRefs;

    return (ruleLevels & m_metadata.level)
        && ((m_resourceId == params.eventResourceId) || params.eventResourceId.isNull())
        && (params.metadata.cameraRefs.empty()
            || (m_metadata.cameraRefs.size() == 1
                && std::find(
                    ruleCameras.begin(),
                    ruleCameras.end(),
                    m_metadata.cameraRefs.front())
                != ruleCameras.end()))
        && checkForKeywords(m_caption, params.caption)
        && checkForKeywords(m_description, params.description);
}

EventParameters PluginEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.eventResourceId = m_resourceId;
    params.caption = m_caption;
    params.description = m_description;
    params.metadata = m_metadata;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
