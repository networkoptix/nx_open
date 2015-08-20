#ifndef __EVENTS_DB_H_
#define __EVENTS_DB_H_

#include <QtSql/QtSql>

#include <core/resource/resource_fwd.h>
#include <business/business_fwd.h>

#include "utils/db/db_helper.h"
#include "api/model/audit/audit_record.h"

class QnTimePeriod;

namespace pb {
    class BusinessActionList;
}

class QnEventsDB: public QnDbHelper
{
public:
    void setEventLogPeriod(qint64 periodUsec);
    bool saveActionToDB(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& actionRes);
    bool removeLogForRes(QnUuid resId);

    QnBusinessActionDataList getActions(
        const QnTimePeriod& period,
        const QnResourceList& resList,
        const QnBusiness::EventType& eventType = QnBusiness::UndefinedEvent, 
        const QnBusiness::ActionType& actionType = QnBusiness::UndefinedAction,
        const QnUuid& businessRuleId = QnUuid()) const;

    void getAndSerializeActions(
        QByteArray& result,
        const QnTimePeriod& period,
        const QnResourceList& resList,
        const QnBusiness::EventType& eventType, 
        const QnBusiness::ActionType& actionType,
        const QnUuid& businessRuleId) const;


    QnAuditRecordList getAuditData(const QnTimePeriod& period, const QnUuid& sessionId = QnUuid());
    int addAuditRecord(const QnAuditRecord& data);
    int updateAuditRecord(int internalId, const QnAuditRecord& data);

    static QnEventsDB* instance();
    static void init();
    static void fini();

    bool createDatabase();

    virtual QnDbTransaction* getTransaction() override;
protected:
    QnEventsDB();

    virtual bool afterInstallUpdate(const QString& updateName) override;
private:
    bool cleanupEvents();
    bool cleanupAuditLog();
    bool migrateBusinessParams();
    QString toSQLDate(qint64 timeMs) const;
    QString getRequestStr(const QnTimePeriod& period,
        const QnResourceList& resList,
        const QnBusiness::EventType& eventType, 
        const QnBusiness::ActionType& actionType,
        const QnUuid& businessRuleId) const;
private:
    qint64 m_lastCleanuptime;
    qint64 m_auditCleanuptime;
    qint64 m_eventKeepPeriod;
    static QnEventsDB* m_instance;
    QnDbTransaction m_tran;
};

#define qnEventsDB QnEventsDB::instance()

#endif // __EVENTS_DB_H_
