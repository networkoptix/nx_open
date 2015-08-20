#include "custom_business_event.h"

QnCustomBusinessEvent::QnCustomBusinessEvent(QnBusiness::EventState toggleState, qint64 timeStamp, const QString& resourceName, const QString& caption, const QString& description):
    base_type(toggleState == QnBusiness::UndefinedState ? QnBusiness::CustomInstantEvent : QnBusiness::CustomProlongedEvent, QnResourcePtr(), toggleState, timeStamp),
    m_resourceName(resourceName),
    m_caption(caption),
    m_description(description)
{
}

bool QnCustomBusinessEvent::checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const {
    bool stateOK =  state == QnBusiness::UndefinedState || state == getToggleState();
    if (!stateOK)
        return false;

    QStringList resourceNameKeywords = params.resourceName.split(L' ', QString::SkipEmptyParts);
    QStringList captionKeywords      = params.caption.split(L' ', QString::SkipEmptyParts);
    QStringList descriptionKeywords  = params.description.split(L' ', QString::SkipEmptyParts);

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

    return mathKeywords(resourceNameKeywords, params.resourceName) &&
           mathKeywords(captionKeywords, params.caption) &&
           mathKeywords(descriptionKeywords, params.description);
}

QnBusinessEventParameters QnCustomBusinessEvent::getRuntimeParams() const {
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.resourceName = m_resourceName;
    params.caption = m_caption;
    params.description = m_description;
    return params;
}
