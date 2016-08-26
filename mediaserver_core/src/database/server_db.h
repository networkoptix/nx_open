#pragma once

#include <chrono>
#include <map>
#include <QtSql/QtSql>

#include <api/model/audit/audit_record.h>

#include <business/business_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <utils/db/db_helper.h>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
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

    using TimePointType = std::chrono::steady_clock::time_point;
    using ResourceActionToTimeMap = std::map<QnUuid, TimePointType>;

public:
    QnServerDb();

    virtual QnDbTransaction* getTransaction() override;

    void setEventLogPeriod(qint64 periodUsec);
    bool saveActionToDB(const QnAbstractBusinessActionPtr& action);
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

    // It does not sort by tags or camera names. Caller should sort it manually
    bool getBookmarks(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);

    bool containsBookmark(const QnUuid &bookmarkId) const;
    QnCameraBookmarkTagList getBookmarkTags(int limit = std::numeric_limits<int>().max());

    bool addBookmark(const QnCameraBookmark &bookmark);
    bool updateBookmark(const QnCameraBookmark &bookmark);
    bool deleteAllBookmarksForCamera(const QString& cameraUniqueId);
    bool deleteBookmark(const QnUuid &bookmarkId);
    bool deleteBookmarksToTime(const QMap<QString, qint64>& dataToDelete);

    bool setLastBackupTime(QnServer::StoragePool pool, const QnUuid& camera,
                           QnServer::ChunksCatalog catalog, qint64 timestampMs);

    qint64 getLastBackupTime(QnServer::StoragePool pool, const QnUuid& camera,
                             QnServer::ChunksCatalog catalog) const;

    void setBookmarkCountController(std::function<void(size_t)> handler);

protected:
    virtual bool afterInstallUpdate(const QString& updateName) override;

    bool addOrUpdateBookmark(const QnCameraBookmark &bookmark, bool isUpdate);
    void updateBookmarkCount();
    bool isActionTooFrequent(const QnUuid& resourceId, qint64 timestamp) const;

private:
    bool createDatabase();
    bool cleanupEvents();
    bool migrateBusinessParamsUnderTransaction();
    bool createBookmarkTagTriggersUnderTransaction();
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
    std::function<void(size_t)> m_updateBookmarkCount;

    mutable ResourceActionToTimeMap m_actionToTime;
    mutable QnMutex m_actionToTimeMutex;
};

#define qnServerDb QnServerDb::instance()
