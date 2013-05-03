#ifndef __EVENTS_DB_H_
#define __EVENTS_DB_H_

#include <QtSql>
#include "business/actions/abstract_business_action.h"
#include "recording/time_period.h"

class QnEventsDB
{
public:
    QnEventsDB();

    void setEventLogPeriod(qint64 periodUsec);
    bool saveActionToDB(QnAbstractBusinessActionPtr action, QnResourcePtr actionRes);
    QList<QnAbstractBusinessActionPtr> getActions(QnTimePeriod period) const;
private:
    bool cleanupEvents();
private:
    QSqlDatabase m_sdb;
    qint64 m_lastCleanuptime;
    qint64 m_eventKeepPeriod;
};

#endif // __EVENTS_DB_H_
