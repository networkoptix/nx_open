#include "conflict_business_event.h"

namespace QnBusinessEventRuntime {

    static QLatin1String sourceStr("source");
    static QLatin1String conflictsStr("conflicts");

    QString getSource(const QnBusinessParams &params) {
        return params[sourceStr].toString();
    }

    void setSource(QnBusinessParams* params, QString value) {
        (*params)[sourceStr] = value;
    }

    QStringList getConflicts(const QnBusinessParams &params) {
        return params[conflictsStr].value<QStringList>();
    }

    void setConflicts(QnBusinessParams* params, QStringList value) {
        (*params)[conflictsStr] = QVariant::fromValue(value);
    }
}

QnConflictBusinessEvent::QnConflictBusinessEvent(const BusinessEventType::Value eventType,
                                                 const QnResourcePtr& resource,
                                                 const qint64 timeStamp,
                                                 const QString& source,
                                                 const QStringList& conflicts):
    base_type(eventType, resource, timeStamp),
    m_source(source),
    m_conflicts(conflicts)
{
}

QnBusinessParams QnConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setSource(&params, m_source);
    QnBusinessEventRuntime::setConflicts(&params, m_conflicts);
    return params;
}

