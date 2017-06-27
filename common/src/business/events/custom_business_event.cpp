#include "custom_business_event.h"
#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>
#include <business/actions/abstract_business_action.h>

QnCustomBusinessEvent::QnCustomBusinessEvent(QnBusiness::EventState toggleState,
                                             qint64 timeStamp, const
                                             QString& resourceName,
                                             const QString& caption,
                                             const QString& description,
                                             QnEventMetaData metadata):
    base_type(QnBusiness::UserDefinedEvent, QnResourcePtr(), toggleState, timeStamp),
    m_resourceName(resourceName),
    m_caption(caption),
    m_description(description),
    m_metadata(std::move(metadata))
{

}

bool QnCustomBusinessEvent::isEventStateMatched(QnBusiness::EventState state, QnBusiness::ActionType actionType) const {
    return state == QnBusiness::UndefinedState || state == getToggleState() || QnBusiness::hasToggleState(actionType);
}

bool QnCustomBusinessEvent::checkEventParams(const QnBusinessEventParameters &params) const
{
    auto unquote = [](const QStringList& dataList)
    {
        QStringList result;
        for (const auto& data: dataList)
            result << nx::utils::unquoteStr(data);
        return result;
    };

    QStringList resourceNameKeywords = unquote(nx::utils::smartSplit(params.resourceName, L' ', QString::SkipEmptyParts));
    QStringList captionKeywords      = unquote(nx::utils::smartSplit(params.caption, L' ', QString::SkipEmptyParts));
    QStringList descriptionKeywords  = unquote(nx::utils::smartSplit(params.description, L' ', QString::SkipEmptyParts));

    auto mathKeywords = [](const QStringList& keywords, const QString& pattern)
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

    return mathKeywords(resourceNameKeywords, m_resourceName) &&
           mathKeywords(captionKeywords, m_caption) &&
           mathKeywords(descriptionKeywords, m_description);
}

QnBusinessEventParameters QnCustomBusinessEvent::getRuntimeParams() const
{
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.resourceName = m_resourceName;
    params.caption = m_caption;
    params.description = m_description;
    params.metadata = m_metadata;
    return params;
}
