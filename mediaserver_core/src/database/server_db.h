#pragma once

#include <QtSql/QtSql>

#include <api/model/audit/audit_record.h>

#include <business/business_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <utils/db/db_helper.h>
#include <utils/common/uuid.h>
#include <utils/common/singleton.h>
#include "server/server_globals.h"

class QnTimePeriod;

namespace pb {
    class BusinessActionList;
}


/** Per-server database. Stores event log, audit data and bookmarks. */
class QnServerDb :
    public QObject,
    public QnDbHelper,
    public Singleton<QnServerDb>
{
    Q_OBJECT
public:
    QnServerDb();

    virtual QnDbTransaction* getTransaction() override;

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

    /* Bookmarks API */

    bool getBookmarks(const QString& cameraUniqueId, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);    
    bool addOrUpdateCameraBookmark(const QnCameraBookmark &bookmark);
    bool removeAllCameraBookmarks(const QString& cameraUniqueId);
    bool deleteCameraBookmark(const QnUuid &bookmarkId);
    bool setLastBackupTime(const QnUuid& camera, QnServer::ChunksCatalog catalog, qint64 timestampMs);
    qint64 getLastBackupTime(const QnUuid& camera, QnServer::ChunksCatalog catalog) const;

protected:
    virtual bool afterInstallUpdate(const QString& updateName) override;
private:
    bool createDatabase();
    bool cleanupEvents();
    bool migrateBusinessParams();
    bool cleanupAuditLog();
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
    QnDbTransaction m_tran;
};

#define qnServerDb QnServerDb::instance()
