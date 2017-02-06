#include "conflict_business_event.h"

const QChar QnConflictBusinessEvent::Delimiter(L'\n');

QnConflictBusinessEvent::QnConflictBusinessEvent(const QnBusiness::EventType eventType,
                                                 const QnResourcePtr& resource,
                                                 const qint64 timeStamp,
                                                 const QString& caption,
                                                 const QString& description):
    base_type(eventType, resource, timeStamp),
    m_caption(caption),
    m_description(description)
{
}

QnBusinessEventParameters QnConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.caption = m_caption;
    params.description = m_description;
    return params;
}

QString QnConflictBusinessEvent::encodeList(const QStringList& values)
{
    QString result;
    for (const auto& value: values) {
        if (!result.isEmpty())
            result.append(Delimiter);
        result.append(value);
    }
    return result;
}
