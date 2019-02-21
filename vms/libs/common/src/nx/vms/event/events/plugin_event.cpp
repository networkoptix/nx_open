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
    const QnUuid& engineResourceId,
    const QString& caption,
    const QString& description,
    nx::vms::api::EventLevel level,
    const QnSecurityCamResourcePtr& device)
    :
    base_type(EventType::pluginEvent, QnResourcePtr(), timeStamp),
    m_resourceId(engineResourceId),
    m_caption(caption),
    m_description(description)
{
    m_metadata.level = level;
    if (!device.isNull())
        m_metadata.cameraRefs.push_back(device->getId().toString());
}

bool PluginEvent::checkEventParams(const EventParameters& params) const
{
    const auto ruleLevels =
        QnLexical::deserialized<nx::vms::api::EventLevels>(params.inputPortId);

    const auto& ruleCameras = params.metadata.cameraRefs;

    // Check Level flag
    const bool isValidLevel = ruleLevels & m_metadata.level;

    // Check Engine Resource id (null is used for 'Any Resource')
    const bool isValidResource = params.eventResourceId.isNull()
        || m_resourceId == params.eventResourceId;

    // Check Camera id (empty list means 'Any Camera')
    const bool isValidCamera = params.metadata.cameraRefs.empty()
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
