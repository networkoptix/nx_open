#include "custom_business_event.h"

QnCustomBusinessEvent::QnCustomBusinessEvent(QnBusiness::EventState toggleState, qint64 timeStamp, const QString& resourceName, const QString& caption, const QString& description):
    base_type(toggleState == QnBusiness::UndefinedState ? QnBusiness::CustomInstantEvent : QnBusiness::CustomProlongedEvent, QnResourcePtr(), toggleState, timeStamp),
    m_resourceName(resourceName),
    m_caption(caption),
    m_description(description)
{
    m_resourceNameKeywords = m_resourceName.split(L' ', QString::SkipEmptyParts);
    m_captionKeywords      = m_caption.split(L' ', QString::SkipEmptyParts);
    m_descriptionKeywords  = m_description.split(L' ', QString::SkipEmptyParts);
}

bool QnCustomBusinessEvent::checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const {
    bool stateOK =  state == QnBusiness::UndefinedState || state == getToggleState();
    if (!stateOK)
        return false;

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

    return mathKeywords(m_resourceNameKeywords, params.resourceName) &&
           mathKeywords(m_captionKeywords, params.caption) &&
           mathKeywords(m_descriptionKeywords, params.description);
}

QnBusinessEventParameters QnCustomBusinessEvent::getRuntimeParams() const {
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.resourceName = m_resourceName;
    params.caption = m_caption;
    params.description = m_description;
    return params;
}
