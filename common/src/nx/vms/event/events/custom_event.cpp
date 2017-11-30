#include "custom_event.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>
#include <nx/vms/event/actions/abstract_action.h>

namespace nx {
namespace vms {
namespace event {

CustomEvent::CustomEvent(
    EventState toggleState,
    qint64 timeStamp, const
    QString& resourceName,
    const QString& caption,
    const QString& description,
    EventMetaData metadata)
    :
    base_type(userDefinedEvent, QnResourcePtr(), toggleState, timeStamp),
    m_resourceName(resourceName),
    m_caption(caption),
    m_description(description),
    m_metadata(std::move(metadata))
{
}

bool CustomEvent::isEventStateMatched(EventState state, ActionType actionType) const
{
    return state == EventState::undefined
        || state == getToggleState()
        || hasToggleState(actionType);
}

bool CustomEvent::checkEventParams(const EventParameters& params) const
{
    return checkForKeywords(m_resourceName, params.resourceName)
        && checkForKeywords(m_caption, params.caption)
        && checkForKeywords(m_description, params.description);
}

EventParameters CustomEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.resourceName = m_resourceName;
    params.caption = m_caption;
    params.description = m_description;
    params.metadata = m_metadata;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
