#pragma once

#include <QtSql/QtSql>

#include <api/model/audit/audit_record.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <utils/db/db_helper.h>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/vms/event/event_fwd.h>
#include <server/server_globals.h>
#include <common/common_module_aware.h>

class QnTimePeriod;
struct QnEventLogRequestData;

namespace pb {
    class BusinessActionList;
}

/** Per-server database. Stores event log, audit data and bookmarks. */
class QnServerDb :
    public QObject,
    public QnCommonModuleAware,
    public QnDbHelper,
    public Singleton<QnServerDb>
{
    Q_OBJECT
public:
    QnServerDb(QnCommonModule* commonModule);

    virtual QnDbTransaction* getTransaction() override;

    bool saveActionToDB(const nx::vms::event::AbstractActionPtr& action);
    bool removeLogForRes(const QnUuid& resId);

    nx::vms::event::ActionDataList getActions(const QnEventLogRequestData& request) const;

    void getAndSerializeActions(const QnEventLogRequestData& request, QByteArray& result) const;

    QnAuditRecordList getAuditData(const QnTimePeriod& period, const QnUuid& sessionId = QnUuid());
    int auditRecordMaxId() const;
    bool addAuditRecords(const std::map<int, QnAuditRecord>& records);

    /* Bookmarks API */

    // It does not sort by tags or camera names. Caller should sort it manually
    bool getBookmarks(const QnSecurityCamResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);

    bool containsBookmark(const QnUuid &bookmarkId) const;
    QnCameraBookmarkTagList getBookmarkTags(int limit = std::numeric_limits<int>().max());

    bool addBookmark(const QnCameraBookmark &bookmark);
    bool updateBookmark(const QnCameraBookmark &bookmark);
    bool deleteAllBookmarksForCamera(const QnUuid& cameraId);
    bool deleteBookmark(const QnUuid &bookmarkId);
    bool deleteBookmarksToTime(const QMap<QnUuid, qint64>& dataToDelete);

    void setBookmarkCountController(std::function<void(size_t)> handler);

    qint64 getLastRemoteArchiveSyncTimeMs(const QnResourcePtr& resource);
    bool updateLastRemoteArchiveSyncTimeMs(const QnResourcePtr& resource, qint64 lastSyncTime);

protected:
    virtual bool afterInstallUpdate(const QString& updateName) override;

    bool addOrUpdateBookmark(const QnCameraBookmark& bookmark, bool isUpdate);
    void updateBookmarkCount();

private:
    bool createDatabase();
    bool cleanupEvents();
    int getRuntimeActionsRecordCount();
    bool migrateBusinessParamsUnderTransaction();
    bool createBookmarkTagTriggersUnderTransaction();
    bool bookmarksUniqueIdToCameraGuid();
    bool cleanupAuditLog();

    QString getRequestStr(const QnEventLogRequestData& request) const;
private:
    qint64 m_lastCleanuptime;
    qint64 m_auditCleanuptime;
    int m_runtimeActionsTotalRecords;
    QnDbTransaction m_tran;
    std::function<void(size_t)> m_updateBookmarkCount;
};

#define qnServerDb QnServerDb::instance()
