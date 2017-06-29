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
    auto unquote =
        [](const QStringList& dataList)
        {
            QStringList result;
            for (const auto& data: dataList)
                result << nx::utils::unquoteStr(data);
            return result;
        };

    QStringList resourceNameKeywords = unquote(
        nx::utils::smartSplit(params.resourceName, L' ', QString::SkipEmptyParts));

    QStringList captionKeywords = unquote(
        nx::utils::smartSplit(params.caption, L' ', QString::SkipEmptyParts));

    QStringList descriptionKeywords = unquote(
        nx::utils::smartSplit(params.description, L' ', QString::SkipEmptyParts));

    auto matchKeywords =
        [](const QStringList& keywords, const QString& pattern)
        {
            if (keywords.isEmpty())
                return true;
            for (const QString& keyword: keywords)
            {
                if (pattern.contains(keyword))
                    return true;
            }
            return false;
        };

    return matchKeywords(resourceNameKeywords, m_resourceName)
        && matchKeywords(captionKeywords, m_caption)
        && matchKeywords(descriptionKeywords, m_description);
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
