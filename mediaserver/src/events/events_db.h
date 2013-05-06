#ifndef __EVENTS_DB_H_
#define __EVENTS_DB_H_

#include <QtSql>
#include "business/actions/abstract_business_action.h"
#include "recording/time_period.h"

class QnEventsDB
{
public:
    void setEventLogPeriod(qint64 periodUsec);
    bool saveActionToDB(QnAbstractBusinessActionPtr action, QnResourcePtr actionRes);
    QList<QnAbstractBusinessActionPtr> getActions(QnTimePeriod period) const;

    static QnEventsDB* instance();
    static void init();
    static void fini();

protected:
    QnEventsDB();
private:
    bool cleanupEvents();
private:
    QSqlDatabase m_sdb;
    qint64 m_lastCleanuptime;
    qint64 m_eventKeepPeriod;
    static QnEventsDB* m_instance;
};

#define qnEventsDB QnEventsDB::instance()

#endif // __EVENTS_DB_H_
