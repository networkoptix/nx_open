#include "plugin_event.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>
#include <nx/vms/event/actions/abstract_action.h>

namespace nx {
namespace vms {
namespace event {

PluginEvent::PluginEvent(
    qint64 timeStamp,
    const QnUuid& pluginInstance,
    const QString& caption,
    const QString& description,
    nx::vms::api::EventLevel level,
    const QStringList& cameraRefs)
    :
    base_type(EventType::pluginEvent, QnResourcePtr(), timeStamp),
    m_resourceId(pluginInstance),
    m_caption(caption),
    m_description(description)
{
    m_metadata.level = level;
    m_metadata.cameraRefs.reserve(cameraRefs.size());
    std::copy(cameraRefs.begin(), cameraRefs.end(), std::back_inserter(m_metadata.cameraRefs));
}

bool PluginEvent::checkEventParams(const EventParameters& params) const
{
    const auto ruleLevels =
        QnLexical::deserialized<nx::vms::api::EventLevels>(params.inputPortId);

    return (ruleLevels & m_metadata.level)
        && ((m_resourceId == params.eventResourceId) || params.eventResourceId.isNull())
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
