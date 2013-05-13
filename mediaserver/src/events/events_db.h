#ifndef __EVENTS_DB_H_
#define __EVENTS_DB_H_

#include <QtSql>
#include "business/actions/abstract_business_action.h"
#include "business/events/abstract_business_event.h"
#include "recording/time_period.h"

namespace pb {
    class BusinessActionList;
}

class QnEventsDB
{
public:
    void setEventLogPeriod(qint64 periodUsec);
    bool saveActionToDB(QnAbstractBusinessActionPtr action, QnResourcePtr actionRes);
    
    QList<QnAbstractBusinessActionPtr> getActions(
        const QnTimePeriod& period,
        const QnId& cameraId = QnId(), 
        const BusinessEventType::Value& eventType = BusinessEventType::NotDefined, 
        const BusinessActionType::Value& actionType = BusinessActionType::NotDefined,
        const QnId& businessRuleId = QnId()) const;

    void QnEventsDB::getAndSerializeActions(
        QByteArray& result,
        const QnTimePeriod& period,
        const QnId& cameraId, 
        const BusinessEventType::Value& eventType, 
        const BusinessActionType::Value& actionType,
        const QnId& businessRuleId) const;


    static QnEventsDB* instance();
    static void init();
    static void fini();

    static void migrate();
protected:
    QnEventsDB();
private:
    bool cleanupEvents();
    QString toSQLDate(qint64 timeMs) const;
private:
    QSqlDatabase m_sdb;
    qint64 m_lastCleanuptime;
    qint64 m_eventKeepPeriod;
    mutable QMutex m_mutex;
    static QnEventsDB* m_instance;
};

#define qnEventsDB QnEventsDB::instance()

#endif // __EVENTS_DB_H_
