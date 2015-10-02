#include "custom_business_event.h"
#include <utils/common/model_functions.h>
#include "network/authutil.h"

QnCustomBusinessEvent::QnCustomBusinessEvent(QnBusiness::EventState toggleState, 
                                             qint64 timeStamp, const 
                                             QString& resourceName, 
                                             const QString& caption, 
                                             const QString& description,
                                             const QnEventMetaData& metadata):
    base_type(QnBusiness::UserDefinedEvent, QnResourcePtr(), toggleState, timeStamp),
    m_resourceName(resourceName),
    m_caption(caption),
    m_description(description),
    m_metadata(metadata)
{
    
}

bool QnCustomBusinessEvent::checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const {
    bool stateOK =  state == QnBusiness::UndefinedState || state == getToggleState();
    if (!stateOK)
        return false;

    QStringList resourceNameKeywords = smartSplit(params.resourceName, L' ');
    QStringList captionKeywords      = smartSplit(params.caption, L' ');
    QStringList descriptionKeywords  = smartSplit(params.description, L' ');

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
