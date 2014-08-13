#ifndef __EVENTS_DB_H_
#define __EVENTS_DB_H_

#include <QtSql/QtSql>
#include "business/actions/abstract_business_action.h"
#include "business/events/abstract_business_event.h"
#include "utils/db/db_helper.h"

class QnTimePeriod;

namespace pb {
    class BusinessActionList;
}

class QnEventsDB: public QnDbHelper
{
public:
    void setEventLogPeriod(qint64 periodUsec);
    bool saveActionToDB(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& actionRes);
    bool removeLogForRes(QUuid resId);

    QList<QnAbstractBusinessActionPtr> getActions(
        const QnTimePeriod& period,
        const QnResourceList& resList,
        const QnBusiness::EventType& eventType = QnBusiness::UndefinedEvent, 
        const QnBusiness::ActionType& actionType = QnBusiness::UndefinedAction,
        const QUuid& businessRuleId = QUuid()) const;

    void getAndSerializeActions(
        QByteArray& result,
        const QnTimePeriod& period,
        const QnResourceList& resList,
        const QnBusiness::EventType& eventType, 
        const QnBusiness::ActionType& actionType,
        const QUuid& businessRuleId) const;


    static QnEventsDB* instance();
    static void init();
    static void fini();

    bool createDatabase();
    static void migrate();
protected:
    QnEventsDB();
private:
    bool cleanupEvents();
    QString toSQLDate(qint64 timeMs) const;
    QString getRequestStr(const QnTimePeriod& period,
        const QnResourceList& resList,
        const QnBusiness::EventType& eventType, 
        const QnBusiness::ActionType& actionType,
        const QUuid& businessRuleId) const;
private:
    qint64 m_lastCleanuptime;
    qint64 m_eventKeepPeriod;
    static QnEventsDB* m_instance;
};

#define qnEventsDB QnEventsDB::instance()

#endif // __EVENTS_DB_H_
