#include "server_db.h"

#include <chrono>

#include <QtCore/QtEndian>

#include <api/helpers/event_log_request_data.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <core/resource/camera_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <media_server/serverutil.h>

#include <recorder/storage_manager.h>
#include <recording/time_period.h>

#include <media_server/settings.h>

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/fusion/model_functions.h>
#include <api/global_settings.h>
#include <common/common_module.h>

#include <nx/vms/server/database/update_helpers/update_analytics_records_helper.h>

using std::chrono::milliseconds;
using namespace std::literals::chrono_literals;

using namespace nx;

namespace {

const char kDelimiter('$');
const char kStringListDelimiter('\n');
static const QString kLastRemoteArchiveSyncTimePropertyName("lastRemoteArchiveSyncTime");

inline int toInt(const QByteArray& ba)
{
    const char* curPtr = ba.data();
    const char* end = curPtr + ba.size();
    int result = 0;
    for (; curPtr < end; ++curPtr)
    {
        if (*curPtr < '0' || *curPtr > '9')
            return result;
        result = result * 10 + (*curPtr - '0');
    }
    return result;
}

inline qint64 toInt64(const QByteArray& ba)
{
    const char* curPtr = ba.data();
    const char* end = curPtr + ba.size();
    qint64 result = 0ll;
    for (; curPtr < end; ++curPtr)
    {
        if (*curPtr < '0' || *curPtr > '9')
            return result;
        result = result*10 + (*curPtr - '0');
    }
    return result;
}

vms::event::ActionParameters convertOldActionParameters(const QByteArray& value)
{
    enum Param
    {
        UrlParam,
        EmailAddressParam,
        UserGroupParam,
        FpsParam,
        QualityParam,
        DurationParam,
        RecordBeforeParam,
        RecordAfterParam,
        RelayOutputIdParam,
        RelayAutoResetTimeoutParam,
        InputPortIdParam,
        KeyParam,
        SayTextParam,
        ParamCount
    };

    vms::event::ActionParameters result;

    if (value.isEmpty())
        return result;

    static const std::vector<QnUuid> kAdminRoles(
        QnUserRolesManager::adminRoleIds().toVector().toStdVector());

    int i = 0;
    int prevPos = -1;
    while (prevPos < value.size() && i < ParamCount)
    {
        int nextPos = value.indexOf(kDelimiter, prevPos+1);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field(value.data() + prevPos + 1, nextPos - prevPos - 1);
        switch ((Param) i)
        {
            case UrlParam:
                result.url = QString::fromUtf8(field.data(), field.size());
                break;
            case EmailAddressParam:
                result.emailAddress = QString::fromUtf8(field.data(), field.size());
                break;
            case UserGroupParam:
                enum { kAdminOnly = 1 };
                if (toInt(field) == kAdminOnly)
                    result.additionalResources = kAdminRoles;
                else
                    result.additionalResources.clear();
                break;
            case FpsParam:
                result.fps = toInt(field);
                break;
            case QualityParam:
                result.streamQuality = static_cast<Qn::StreamQuality>(toInt(field));
                break;
            case DurationParam:
                result.durationMs = toInt(field) * 1000;
                break;
            case RecordBeforeParam:
                break;
            case RecordAfterParam:
                result.recordAfter = toInt(field);
                break;
            case RelayOutputIdParam:
                result.relayOutputId = QString::fromUtf8(field.data(), field.size());
                break;
            case RelayAutoResetTimeoutParam:
                result.durationMs = toInt(field);
                break;
            case InputPortIdParam:
                break;
            case KeyParam:
                break;
            case SayTextParam:
                result.sayText = QString::fromUtf8(field.data(), field.size());
                break;
            default:
                break;
        }
        prevPos = nextPos;
        i++;
    }

    return result;
}

vms::event::EventParameters convertOldEventParameters(
    const QByteArray& value, QnUuid* actionResourceId)
{
    enum Param
    {
        EventTypeParam,
        EventTimestampParam,
        EventResourceParam,
        ActionResourceParam,
        InputPortIdParam,
        ReasonCodeParam,
        ReasonParamsEncodedParam,
        SourceParam,
        ConflictsParam,
        ParamCount
    };

    vms::event::EventParameters result;

    if (value.isEmpty())
        return result;

    int i = 0;
    int prevPos = -1;
    while (prevPos < value.size() && i < ParamCount)
    {
        int nextPos = value.indexOf(kDelimiter, prevPos+1);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field = QByteArray::fromRawData(
            value.data() + prevPos + 1, nextPos - prevPos - 1);
        if (!field.isEmpty())
        {
            switch ((Param) i)
            {
                case EventTypeParam:
                    result.eventType = (vms::api::EventType) toInt(field);
                    break;
                case EventTimestampParam:
                    result.eventTimestampUsec = toInt64(field);
                    break;
                case EventResourceParam:
                    result.eventResourceId = QnUuid(field);
                    break;
                case ActionResourceParam:
                    *actionResourceId = QnUuid(field);
                    break;
                case InputPortIdParam:
                    result.inputPortId = QString::fromUtf8(field.data(), field.size());
                    break;
                case ReasonCodeParam:
                    result.reasonCode = (vms::api::EventReason) toInt(field);
                    break;
                case ReasonParamsEncodedParam:
                {
                    auto value = QString::fromUtf8(field.data(), field.size());
                    if (!value.isEmpty())
                        result.description = value;
                    break;
                }
                case SourceParam:
                    result.caption = QString::fromUtf8(field.data(), field.size());
                    break;
                case ConflictsParam:
                {
                    auto value = QString::fromLatin1(field.data(), field.size());
                    if (!value.isEmpty())
                        result.description = value;
                    break;
                }
                default:
                    break;
            }
        }

        ++i;
        prevPos = nextPos;
    }

    return result;
}

int getBookmarksQueryLimit(const QnCameraBookmarkSearchFilter& filter)
{
    if (filter.sparsing.used)
        return QnCameraBookmarkSearchFilter::kNoLimit;

    switch (filter.orderBy.column)
    {
        case Qn::BookmarkName:
        case Qn::BookmarkStartTime:
        case Qn::BookmarkDuration:
            return filter.limit;

        case Qn::BookmarkCreationTime:
        case Qn::BookmarkCreator:
        case Qn::BookmarkCameraName:
        case Qn::BookmarkTags:
            // No limit for manually sorted sequences.
            return QnCameraBookmarkSearchFilter::kNoLimit;

        default:
            NX_ASSERT(false, "Invalid sorting column value!");
            return QnCameraBookmarkSearchFilter::kNoLimit;
    }
}

} // namespace

static const qint64 kDbCleanupIntervalUs = 1000000ll * 3600;

QnServerDb::QnServerDb(QnMediaServerModule* serverModule)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_lastCleanuptimeUs(0),
    m_auditCleanuptimeUs(0),
    m_runtimeActionsTotalRecords(0),
    m_tran(m_sdb, m_mutex)
{
}

bool QnServerDb::open()
{
    const QString eventsDBFilePath = serverModule()->settings().eventsDBFilePath();
    const QString fileName = closeDirPath(eventsDBFilePath)
        + QString(lit("mserver.sqlite"));

    QString connectionName = "QnServerDb" + serverModule()->commonModule()->moduleGUID().toString();
    addDatabase(fileName, connectionName);
    if (m_sdb.open())
    {
        if (!createDatabase()) // Create tables if DB is empty.
            qWarning() << "Cannot create tables for sqlLite database";
        else
            m_runtimeActionsTotalRecords = getRuntimeActionsRecordCount();
    }
    else
    {
        NX_WARNING(this, "Cannot create sqlLite database %1", fileName);
        return false;
    }

    if (!execSQLScript("vacuum;", m_sdb))
    {
        qWarning() << "failed to vacuum mserver database" << Q_FUNC_INFO;
        return false;
    }

    if (!tuneDBAfterOpen(&m_sdb))
    {
        qWarning() << "failed to turn on journal mode for mserver database" << Q_FUNC_INFO;
        return false;
    }

    return true;
}

QnServerDb::QnDbTransaction* QnServerDb::getTransaction()
{
    return &m_tran;
}

bool QnServerDb::createDatabase()
{
    QnDbTransactionLocker tran(getTransaction());

    QSqlQuery versionQuery(m_sdb);
    versionQuery.prepare("SELECT sql from sqlite_master where name = 'runtime_actions'");
    if (versionQuery.exec() && versionQuery.next())
    {
        QByteArray sql = versionQuery.value("sql").toByteArray();
        versionQuery.clear();
        if (!sql.contains("business_rule_guid"))
        {
            if (!execSQLQuery("drop index 'timeAndCamIdx'", m_sdb, Q_FUNC_INFO))
                return false;
            if (!execSQLQuery("drop table 'runtime_actions'", m_sdb, Q_FUNC_INFO))
                return false;
        }
    }

    if (!isObjectExists(lit("table"), lit("runtime_actions"), m_sdb))
    {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE TABLE \"runtime_actions\" "
            "(timestamp INTEGER NOT NULL, action_type SMALLINT NOT NULL, "
            "action_params TEXT, runtime_params TEXT, business_rule_guid BLOB(16), toggle_state SMALLINT, aggregation_count INTEGER, "
            "event_type SMALLINT, event_resource_GUID BLOB(16), action_resource_guid BLOB(16))"
        );
        if (!execSQLQuery(&ddlQuery, Q_FUNC_INFO))
            return false;
    }

    if (!isObjectExists(lit("index"), lit("timeAndCamIdx"), m_sdb))
    {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE INDEX \"timeAndCamIdx\" ON \"runtime_actions\" (timestamp,event_resource_guid)"
        );
        if (!execSQLQuery(&ddlQuery, Q_FUNC_INFO))
            return false;
    }

    if (!applyUpdates(":/mserver_updates"))
        return false;

    if (!isObjectExists(lit("table"), lit("audit_log"), m_sdb))
    {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE TABLE \"audit_log\" ("
            "id INTEGER NOT NULL PRIMARY KEY autoincrement,"
            "createdTimeSec INTEGER NOT NULL,"
            "rangeStartSec INTEGER NOT NULL,"
            "rangeEndSec INTEGER NOT NULL,"
            "eventType SMALLINT NOT NULL,"
            "resources BLOB,"
            "params TEXT,"
            "authSession TEXT)"
        );
        if (!execSQLQuery(&ddlQuery, Q_FUNC_INFO))
            return false;
    }

    if (!isObjectExists(lit("index"), lit("auditTimeIdx"), m_sdb))
    {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE INDEX \"auditTimeIdx\" ON \"audit_log\" (createdTimeSec)"
        );
        if (!execSQLQuery(&ddlQuery, Q_FUNC_INFO))
            return false;
    }

    if (!tran.commit())
    {
        qWarning() << Q_FUNC_INFO << m_sdb.lastError().text();
        return false;
    }

    return true;
}

int QnServerDb::auditRecordMaxId() const
{
    QnWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return -1;

    QSqlQuery query(m_sdb);
    query.prepare("select max(id) from audit_log");
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return -1;
    query.next();
    return query.value(0).toInt();
}

bool QnServerDb::addAuditRecords(const std::map<int, QnAuditRecord>& records)
{
    QnDbTransactionLocker tran(getTransaction());

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO audit_log"
        "(id, createdTimeSec, rangeStartSec, rangeEndSec, eventType, resources, params, authSession)"
        "VALUES"
        "(:id, :createdTimeSec, :rangeStartSec, :rangeEndSec, :eventType, :resources, :params, :authSession)"
    );

    for (auto itr = records.begin(); itr != records.end(); ++itr)
    {
        const auto& data = itr->second;
        NX_ASSERT(data.eventType != Qn::AR_NotDefined);
        NX_ASSERT((data.eventType & (data.eventType - 1)) == 0);

        insQuery.bindValue(":id", itr->first);
        QnSql::bind(data, &insQuery);
        if (!execSQLQuery(&insQuery, Q_FUNC_INFO))
            return false;

        NX_VERBOSE(this, lm("Update audit record %1 for %2 (session %3): %4 %5-%6").args(
            itr->first, data.authSession.userName, data.authSession.id,
            QnLexical::serialized(data.eventType), data.rangeStartSec, data.rangeEndSec));
    }

    cleanupAuditLog();
    tran.commit();
    return true;
}

bool QnServerDb::closeUnclosedAuditRecords(int lastRunningTimeSec)
{
    QnDbTransactionLocker tran(getTransaction());
    if (!m_sdb.isOpen())
        return false;

    QSqlQuery query(m_sdb);
    if (lastRunningTimeSec)
    {
        query.prepare(R"sql(
            UPDATE audit_log
            SET rangeEndSec = :lastRunningTimeSec
            WHERE (rangeStartSec != 0 AND rangeEndSec = 0)
        )sql");

        query.bindValue(":lastRunningTimeSec", lastRunningTimeSec);
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;

        NX_DEBUG(this, lm("Close %1 audit records with last runnig time %2").args(
            query.numRowsAffected(), lastRunningTimeSec));
    }

    query.prepare(R"sql(
        UPDATE audit_log
        SET rangeEndSec = rangeStartSec
        WHERE rangeStartSec > rangeEndSec
    )sql");

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    NX_DEBUG(this, lm("Fixed %1 incorrcet audit record end times").args(query.numRowsAffected()));
    return tran.commit();
}

QnAuditRecordList QnServerDb::getAuditData(const QnTimePeriod& period, const QnUuid& sessionId)
{
    QnAuditRecordList result;
    QString request = QString::fromLatin1(
        "SELECT * "
        "FROM audit_log "
        "WHERE createdTimeSec BETWEEN ? and ? ");
    if (!sessionId.isNull())
        request += lit("AND authSession like '%1%'").arg(sessionId.toString());
    request += lit("ORDER BY id");

    QnWriteLocker lock(&m_mutex);
    QSqlQuery query(m_sdb);
    query.prepare(request);
    query.addBindValue(period.startTimeMs / 1000);
    query.addBindValue(period.endTimeMs() == DATETIME_NOW ? INT_MAX : period.endTimeMs() / 1000);

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return result;

    QnSql::fetch_many(query, &result);

    return result;
}

int QnServerDb::getRuntimeActionsRecordCount()
{
    int currentRecordCount = -1;
    QSqlQuery countQuery(m_sdb);
    countQuery.prepare("SELECT Count(*) FROM runtime_actions");
    bool rez = execSQLQuery(&countQuery, Q_FUNC_INFO);

    if (!rez)
        return currentRecordCount;

    countQuery.next();
    currentRecordCount = countQuery.value(0).toInt();

    return currentRecordCount;
};

bool QnServerDb::cleanupEvents()
{
    bool rez = true;

    // cleanup by time
    qint64 currentTimeUs = qnSyncTime->currentUSecsSinceEpoch();
    if (currentTimeUs - m_lastCleanuptimeUs > kDbCleanupIntervalUs)
    {
        m_lastCleanuptimeUs = currentTimeUs;
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM runtime_actions where timestamp < :timestamp");
        int utcMs = currentTimeUs / 1000LL - globalSettings()->eventLogPeriodDays() * 3600 * 24 * 1000LL;

        delQuery.bindValue(":timestamp", utcMs);
        rez = execSQLQuery(&delQuery, Q_FUNC_INFO);

        if (rez)
            m_runtimeActionsTotalRecords -= delQuery.numRowsAffected();
    }

    // cleanup by  record count
    const int maxRecords = globalSettings()->maxEventLogRecords();
    const int maxOverflowRecords = int(maxRecords * 1.2);

    if (maxOverflowRecords < m_runtimeActionsTotalRecords)
    {
        QSqlQuery cleanupQuery(m_sdb);
        cleanupQuery.prepare(
            "DELETE FROM runtime_actions WHERE rowid in \
            (SELECT rowid FROM runtime_actions ORDER BY rowid LIMIT :recordsToDelete)"
        );
        cleanupQuery.bindValue(":recordsToDelete", m_runtimeActionsTotalRecords - maxRecords);
        rez = execSQLQuery(&cleanupQuery, Q_FUNC_INFO);

        if (rez)
            m_runtimeActionsTotalRecords -= cleanupQuery.numRowsAffected();
    }

    return rez;
}

bool QnServerDb::migrateBusinessParamsUnderTransaction()
{
    struct RowParams
    {
        QByteArray actionParams;
        QByteArray runtimeParams;
    };

    auto convertAction =
        [](const QByteArray &packed, const QnUuid& actionResourceId) -> QByteArray
        {
            // Check if data is in Ubjson already.
            if (!packed.isEmpty() && packed[0] == L'[')
                return packed;
            vms::event::ActionParameters ap = convertOldActionParameters(packed);
            ap.actionResourceId = actionResourceId;
            return QnUbjson::serialized(ap);
        };

    auto convertRuntime =
        [](const QByteArray &packed, QnUuid* actionResourceId)-> QByteArray
        {
            // Check if data is in Ubjson already.
            if (!packed.isEmpty() && packed[0] == L'[')
                return packed;
            vms::event::EventParameters rp = convertOldEventParameters(packed, actionResourceId);
            return QnUbjson::serialized(rp);
        };

    QMap<int, RowParams> remapData;

    {
        // Reading data from the table.
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare(
            "SELECT rowid, action_params, runtime_params FROM runtime_actions order by rowid");
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;

        while (query.next())
        {
            qint32 id = query.value(0).toInt();
            QByteArray actionData = query.value(1).toByteArray();
            QByteArray runtimeData = query.value(2).toByteArray();

            RowParams remappedData;
            QnUuid actionResourceId;
            // Move actionResourceId from runtimeParams to actionParams.
            remappedData.runtimeParams = convertRuntime(runtimeData, &actionResourceId);
            remappedData.actionParams = convertAction(actionData, actionResourceId);
            remapData[id] = remappedData;
        }
    }

    {
        QSqlQuery query(m_sdb);
        query.prepare(
            "UPDATE runtime_actions SET action_params = :action, runtime_params = :runtime \
            WHERE rowid = :rowid");
        for (auto iter = remapData.cbegin(); iter != remapData.cend(); ++iter)
        {
            query.bindValue(":rowid", iter.key());
            query.bindValue(":action", iter->actionParams);
            query.bindValue(":runtime", iter->runtimeParams);
            if (!execSQLQuery(&query, Q_FUNC_INFO))
                return false;
        }
    }

    return true;
}

bool QnServerDb::bookmarksUniqueIdToCameraGuid()
{
    QMap<QnUuid, QString> uniqueIdByGuid;

    // Build uniqueIdByGuid.
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(
        "SELECT guid, unique_id FROM bookmarks_tmp");
    if (!query.exec())
        return false;
    while (query.next())
    {
        const QnUuid guid = QnSql::deserialized_field<QnUuid>(query.value(0));
        const QString uniqueId = query.value(1).toString();
        uniqueIdByGuid.insert(guid, uniqueId);
    }

    // Change uniqueId to cameraId: cameraId = md5(uniqueId).
    for (auto it = uniqueIdByGuid.begin(); it != uniqueIdByGuid.end(); ++it)
    {
        const QnUuid cameraId = QnSecurityCamResource::makeCameraIdFromUniqueId(it.value());

        QSqlQuery query(m_sdb);
        query.prepare(
            "UPDATE bookmarks SET camera_guid = :cameraId WHERE guid = :guid");
        query.bindValue(":guid", QnSql::serialized_field(it.key()));
        query.bindValue(":cameraId", QnSql::serialized_field(cameraId));
        if (!query.exec())
        {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool QnServerDb::updateAnalyticsEventRecords()
{
    nx::vms::server::database::UpdateAnalyticsRecordsHelper helper(m_sdb);
    return helper.doUpdate();
}

bool QnServerDb::createBookmarkTagTriggersUnderTransaction()
{
    // ATTENTION: Do not attempt to move this code to sql scripts. It uses semicolons inside SQL
    // queries, thus these queries cannot be read by our primitive lexical sql script parser.

    {
        QString queryStr =
            "CREATE TRIGGER increment_bookmark_tag_counter AFTER INSERT ON bookmark_tags "
            "BEGIN "
                "INSERT OR IGNORE INTO bookmark_tag_counts (tag, count) VALUES (NEW.name, 0); "
                "UPDATE bookmark_tag_counts SET count = count + 1 WHERE tag = NEW.name; "
            "END; ";
        if (!execSQLQuery(queryStr, m_sdb, Q_FUNC_INFO))
            return false;
    }

    {
        QString queryStr =
            "CREATE TRIGGER decrement_bookmark_tag_counter AFTER DELETE ON bookmark_tags "
            "BEGIN "
                "UPDATE bookmark_tag_counts SET count = count - 1 WHERE tag = OLD.name; "
                "DELETE FROM bookmark_tag_counts WHERE tag = OLD.name AND count <= 0; "
            "END; ";
        if (!execSQLQuery(queryStr, m_sdb, Q_FUNC_INFO))
            return false;
    }

    return true;
}

bool QnServerDb::cleanupAuditLog()
{
    bool rez = true;

    qint64 currentTimeUs = qnSyncTime->currentUSecsSinceEpoch();
    if (currentTimeUs - m_auditCleanuptimeUs > kDbCleanupIntervalUs)
    {
        m_auditCleanuptimeUs = currentTimeUs;
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM audit_log where createdTimeSec < :createdTimeSec");
        int utc = currentTimeUs / 1000000ll - globalSettings()->auditTrailPeriodDays() * 3600 * 24;
        delQuery.bindValue(":createdTimeSec", utc);
        rez = execSQLQuery(&delQuery, Q_FUNC_INFO);
    }
    return rez;
}

bool QnServerDb::removeLogForRes(const QnUuid& resId)
{
    QnWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery delQuery(m_sdb);
    delQuery.prepare(R"(
        DELETE FROM runtime_actions where event_resource_guid = :id1 or action_resource_guid = :id2
    )");

    delQuery.bindValue(":id1", resId.toRfc4122());
    delQuery.bindValue(":id2", resId.toRfc4122());

    bool rez = execSQLQuery(&delQuery, Q_FUNC_INFO);
    if (rez)
        m_runtimeActionsTotalRecords -= delQuery.numRowsAffected();

    return rez;
}

bool QnServerDb::saveActionToDB(const vms::event::AbstractActionPtr& action)
{
    QnWriteLocker lock(&m_mutex);

    if (action->isReceivedFromRemoteHost())
        return false; //< Server should save the action before proxing locally.

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare(R"(
        INSERT INTO runtime_actions (
            timestamp,
            action_type,
            action_params,
            runtime_params,
            business_rule_guid,
            toggle_state,
            aggregation_count,
            event_type,
            event_subtype,
            event_resource_guid,
            action_resource_guid)
        VALUES (
            :timestamp,
            :action_type,
            :action_params,
            :runtime_params,
            :business_rule_guid,
            :toggle_state,
            :aggregation_count,
            :event_type,
            :event_subtype,
            :event_resource_guid,
            :action_resource_guid);
    )");

    qint64 timestampUsec = action->getRuntimeParams().eventTimestampUsec;
    QnUuid eventResId = action->getRuntimeParams().eventResourceId;
    QString eventSubtype;
    if (action->getRuntimeParams().eventType == vms::api::EventType::analyticsSdkEvent)
        eventSubtype = action->getRuntimeParams().getAnalyticsEventTypeId();

    const auto actionParams = action->getParams();

    insQuery.bindValue(":timestamp", timestampUsec/1000);
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", QnUbjson::serialized(actionParams));
    insQuery.bindValue(":runtime_params", QnUbjson::serialized(action->getRuntimeParams()));
    insQuery.bindValue(":business_rule_guid", action->getRuleId().toRfc4122());
    insQuery.bindValue(":toggle_state", (int) action->getToggleState());
    insQuery.bindValue(":aggregation_count", action->getAggregationCount());

    insQuery.bindValue(":event_type", (int) action->getRuntimeParams().eventType);
    insQuery.bindValue(":event_subtype", eventSubtype);
    insQuery.bindValue(":event_resource_guid", eventResId.toRfc4122());
    bindId(&insQuery, ":action_resource_guid", actionParams.actionResourceId);

    bool rez = execSQLQuery(&insQuery, Q_FUNC_INFO);
    if (rez)
    {
        m_runtimeActionsTotalRecords += insQuery.numRowsAffected();
        cleanupEvents();
    }

    return rez;
}

QString QnServerDb::getRequestStr(const QnEventLogFilterData& request,
    Qt::SortOrder order,
    int limit) const
{
    QString requestStr(lit("SELECT * FROM runtime_actions WHERE"));
    if (!request.period.isInfinite())
    {
        requestStr += lit(" timestamp BETWEEN '%1' AND '%2'")
            .arg(request.period.startTimeMs).arg(request.period.endTimeMs());
    }
    else
    {
        requestStr += lit(" timestamp >= '%1'").arg(request.period.startTimeMs);
    }

    if (request.cameras.size() == 1)
    {
        requestStr += QString(lit(" AND event_resource_guid = %1 "))
            .arg(guidToSqlString(request.cameras[0]->getId()));
    }
    else if (request.cameras.size() > 1)
    {
        QString idList;
        for (const auto& camera: request.cameras)
        {
            if (!idList.isEmpty())
                idList += QLatin1Char(',');
            idList += guidToSqlString(camera->getId());
        }
        requestStr += QString(lit(" AND event_resource_guid IN (%1) ")).arg(idList);
    }

    if (request.eventType != vms::api::EventType::undefinedEvent
        && request.eventType != vms::api::EventType::anyEvent)
    {
        if (vms::event::hasChild(request.eventType))
        {
            QList<vms::api::EventType> events = vms::event::childEvents(request.eventType);
            QString eventTypeStr;
            for(vms::api::EventType evnt: events) {
                if (!eventTypeStr.isEmpty())
                    eventTypeStr += QLatin1Char(',');
                eventTypeStr += QString::number((int) evnt);
            }
            requestStr += QString(lit(" AND event_type IN (%1) ")).arg(eventTypeStr);
        }
        else
        {
            requestStr += QString(lit(" AND event_type = %1 ")).arg((int) request.eventType);
        }
    }

    if (!request.eventSubtype.isEmpty())
    {
        requestStr += lit(" AND event_subtype = '%1' ").arg(request.eventSubtype);
    }

    if (request.actionType != vms::api::ActionType::undefinedAction)
        requestStr += QString(lit(" AND action_type = %1 ")).arg((int) request.actionType);

    if (!request.ruleId.isNull())
    {
        requestStr += QString(lit(" AND  business_rule_guid = %1 "))
            .arg(guidToSqlString(request.ruleId));
    }

    requestStr += lit(" ORDER BY rowid");

    if (order == Qt::DescendingOrder)
        requestStr += lit(" DESC");

    if (limit > 0 && limit < std::numeric_limits<int>().max())
        requestStr += lit(" LIMIT %1").arg(limit);

    return requestStr;
}

vms::event::ActionDataList QnServerDb::getActions(
    const QnEventLogFilterData& request,
    Qt::SortOrder order,
    int limit) const
{
    vms::event::ActionDataList result;
    QString requestStr = getRequestStr(request, order, limit);

    QnWriteLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare(requestStr);
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return result;

    QSqlRecord rec = query.record();
    int actionTypeIdx = rec.indexOf("action_type");
    int actionParamIdx = rec.indexOf("action_params");
    int runtimeParamIdx = rec.indexOf("runtime_params");
    int businessRuleIdx = rec.indexOf("business_rule_guid");
    int aggregationCntIdx = rec.indexOf("aggregation_count");

    while (query.next())
    {
        vms::event::ActionData actionData;

        actionData.actionType = (vms::api::ActionType) query.value(actionTypeIdx).toInt();
        actionData.actionParams = QnUbjson::deserialized<vms::event::ActionParameters>(
            query.value(actionParamIdx).toByteArray());
        actionData.eventParams = QnUbjson::deserialized<vms::event::EventParameters>(
            query.value(runtimeParamIdx).toByteArray());
        actionData.businessRuleId = QnUuid::fromRfc4122(
            query.value(businessRuleIdx).toByteArray());
        actionData.aggregationCount = query.value(aggregationCntIdx).toInt();

        if (actionData.eventParams.eventType == vms::api::cameraMotionEvent
            || actionData.eventParams.eventType == vms::api::cameraInputEvent
            || actionData.eventParams.eventType == vms::api::analyticsSdkEvent
            || actionData.actionType == vms::api::bookmarkAction
            || actionData.actionType == vms::api::ActionType::acknowledgeAction
            || actionData.actionType == vms::api::ActionType::fullscreenCameraAction
            )
        {
            QnNetworkResourcePtr camRes =
                resourcePool()->getResourceById<QnNetworkResource>(actionData.eventParams.eventResourceId);
            if (camRes)
            {
                if (QnStorageManager::isArchiveTimeExists(
                    serverModule(),
                    camRes->getUniqueId(), actionData.eventParams.eventTimestampUsec / 1000))
                {
                    actionData.flags |= vms::event::ActionData::VideoLinkExists;
                }
            }
        }
        result.push_back(std::move(actionData));
    }

    return result;
}

inline void appendIntToByteArray(QByteArray& byteArray, int value)
{
    value = qToBigEndian(value);
    byteArray.append((const char*) &value, sizeof(int));
}

inline void appendQnUuidToByteArray(QByteArray& byteArray, const QnUuid& value)
{
    byteArray.append(value.toRfc4122());
}

void QnServerDb::getAndSerializeActions(const QnEventLogRequestData& request,
    QByteArray& result) const
{
    QString requestStr = getRequestStr(request.filter);

    QnWriteLocker lock(&m_mutex);

    QSqlQuery actionsQuery(m_sdb);
    actionsQuery.prepare(requestStr);
    if (!execSQLQuery(&actionsQuery, Q_FUNC_INFO))
        return;

    QSqlRecord rec = actionsQuery.record();
    int actionTypeIdx = rec.indexOf(lit("action_type"));
    int actionParamIdx = rec.indexOf(lit("action_params"));
    int runtimeParamIdx = rec.indexOf(lit("runtime_params"));
    int businessRuleIdx = rec.indexOf(lit("business_rule_guid"));
    int aggregationCntIdx = rec.indexOf(lit("aggregation_count"));
    int eventTypeIdx = rec.indexOf(lit("event_type"));
    int eventResIdx = rec.indexOf(lit("event_resource_guid"));
    int timestampIdx = rec.indexOf(lit("timestamp"));
    rec.field(timestampIdx).setType(QVariant::LongLong);

    int sizeField = 0;
    result.append((const char *) &sizeField, sizeof(int));

    while (actionsQuery.next())
    {
        int flags = 0;
        const auto eventType = (vms::api::EventType) actionsQuery.value(eventTypeIdx).toInt();
        const auto actionType = (vms::api::ActionType) actionsQuery.value(actionTypeIdx).toInt();
        if (eventType == vms::api::EventType::cameraMotionEvent
            || eventType == vms::api::EventType::cameraInputEvent
            || actionType == vms::api::ActionType::bookmarkAction
            || actionType == vms::api::ActionType::acknowledgeAction
            || actionType == vms::api::ActionType::fullscreenCameraAction
            )
        {
            QnUuid eventResId = QnUuid::fromRfc4122(actionsQuery.value(eventResIdx).toByteArray());
            QnNetworkResourcePtr camRes =
                resourcePool()->getResourceById<QnNetworkResource>(eventResId);
            if (camRes)
            {
                if (QnStorageManager::isArchiveTimeExists(
                    serverModule(),
                    camRes->getUniqueId(), actionsQuery.value(timestampIdx).toInt() * 1000ll))
                {
                    flags |= vms::event::ActionData::VideoLinkExists;
                }
            }
        }

        appendIntToByteArray(result, flags);
        appendIntToByteArray(result, actionsQuery.value(actionTypeIdx).toInt());
        result.append(actionsQuery.value(businessRuleIdx).toByteArray());
        appendIntToByteArray(result, actionsQuery.value(aggregationCntIdx).toInt());

        QByteArray runtimeParams = actionsQuery.value(runtimeParamIdx).toByteArray();
        appendIntToByteArray(result, runtimeParams.size());
        result.append(runtimeParams);

        QByteArray actionParams = actionsQuery.value(actionParamIdx).toByteArray();
        appendIntToByteArray(result, actionParams.size());
        result.append(actionParams);

        ++sizeField;
    }
    sizeField = qToBigEndian(sizeField);
    memcpy(result.data(), &sizeField, sizeof(int));
}

bool QnServerDb::afterInstallUpdate(const QString& updateName)
{
    if (updateName.endsWith(lit("/01_business_params.sql")))
        return migrateBusinessParamsUnderTransaction();

    if (updateName.endsWith(lit("/03_add_bookmark_tag_counts_and_rename_tables.sql")))
        return createBookmarkTagTriggersUnderTransaction();

    if (updateName.endsWith(lit("/07_1_bookmarks_unique_id_to_camera_guid.sql")))
        return bookmarksUniqueIdToCameraGuid();

    if (updateName.endsWith("/16_update_analytics_event_records.sql"))
        return updateAnalyticsEventRecords();

    return true;
}

bool QnServerDb::getBookmarks(
    const QnSecurityCamResourceList& cameras,
    const QnCameraBookmarkSearchFilter& filter,
    QnCameraBookmarkList& result)
{
    QList<QnUuid> cameraIds;
    for (const auto& camera: cameras)
    {
        if (camera && !camera->getId().isNull())
            cameraIds << camera->getId();
    }
    return getBookmarks(cameraIds, filter, result);
}


bool QnServerDb::getMaxBookmarksMaxDurationMs(
    const QList<QnUuid>& cameraIds,
    const QnCameraBookmarkSearchFilter& filter,
    qint64* outResult) const
{
    *outResult = 0;

    QSet<QString> cameraStringIds;
    for (const auto& id: cameraIds)
        cameraStringIds << guidToSqlString(id);
    static const QString queryTemplate = QString(R"(SELECT max(duration) FROM bookmarks WHERE %1)");

    QList<QVariant> bindings;

    QString filterText = lit("camera_guid in (%1)").arg(cameraStringIds.toList().join(","));

    const auto queryStr = queryTemplate.arg(filterText);

    QnWriteLocker lock(&m_mutex);
    QSqlQuery query(m_sdb);

    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    for (const auto& b: bindings)
        query.addBindValue(b);

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    if (query.next())
        *outResult = query.value(0).toLongLong();
    return true;
}

bool QnServerDb::getBookmarks(
    const QList<QnUuid>& cameraIds,
    const QnCameraBookmarkSearchFilter& constFilter,
    QnCameraBookmarkList& result)
{
    QnCameraBookmarkSearchFilter filter(constFilter);

    const bool hasStartTimeLimit = filter.startTimeMs.count() > 0;
    const bool hasEndTimeLimit = filter.endTimeMs.count() < std::numeric_limits<qint64>().max();

    bool isRangeClosed = hasStartTimeLimit && hasEndTimeLimit;
    if (isRangeClosed)
    {
        QnMultiServerCameraBookmarkList bookmarks;

        // Optimization. Do two separate SQL requests to force SqLite use index.
        bookmarks.resize(2);

        // Bookmarks that starts before startTime but ends after startTime
        if (!getBookmarksInternal(cameraIds, filter, bookmarks[1], /*isAdditionRangeRequest*/ true))
            return false;

        QnCameraBookmark::sortBookmarks(
            serverModule()->commonModule(),
            bookmarks[1],
            QnBookmarkSortOrder(Qn::BookmarkCameraThenStartTime, filter.orderBy.order));

        // Bookmarks between start end end time
        bool needSecondRequest = true;
        if (filter.limit > 0)
        {
            filter.limit -= bookmarks[1].size();
            if (filter.limit <= 0)
                needSecondRequest = false;
        }

        if (!getBookmarksInternal(cameraIds, filter, bookmarks[0], /*isAdditionRangeRequest*/ false))
            return false;

        result = QnCameraBookmark::mergeCameraBookmarks(
            serverModule()->commonModule(),
            bookmarks,
            QnBookmarkSortOrder(Qn::BookmarkCameraThenStartTime, filter.orderBy.order));
    }
    else
    {
        if (!getBookmarksInternal(cameraIds, filter, result, false))
            return false;
    }

    QnCameraBookmark::sortBookmarks(serverModule()->commonModule(), result, filter.orderBy);
    return true;

}

bool QnServerDb::getBookmarksInternal(
    const QList<QnUuid>& cameraIds,
    const QnCameraBookmarkSearchFilter& filter,
    QnCameraBookmarkList& result,
    bool isAdditionRangeRequest)
{
    const bool hasStartTimeLimit = filter.startTimeMs.count() > 0;
    const bool hasEndTimeLimit = filter.endTimeMs.count() < std::numeric_limits<qint64>().max();

    QString queryTemplate = QString(R"(
        SELECT
            guid as guid,
            name as name,
            start_time as startTimeMs,
            duration as durationMs,
            end_time as endTimeMs,
            description as description,
            timeout as timeout,
            camera_guid as cameraId,
            creator_guid as creatorId,
            created as creationTimeStampMs,
            (SELECT group_concat(name) FROM bookmark_tags t where t.bookmark_guid = guid) as tags
        FROM bookmarks
        WHERE %1
        ORDER BY camera_guid %2,
    )");
    if (isAdditionRangeRequest)
        queryTemplate += " end_time %2";
    else
        queryTemplate += " start_time %2";

    QList<QVariant> bindings;

    QSet<QString> cameraStringIds;
    for (const auto& id: cameraIds)
        cameraStringIds << guidToSqlString(id);
    QString filterText = lit("camera_guid in (%1)").arg(cameraStringIds.toList().join(","));

    auto addFilter = [&](
        const QString& filter,
        const QVariant& value,
        const QVariant& value2 = QVariant())
    {
        filterText += lit(" AND %1").arg(filter);
        bindings.push_back(value);
        if (!value2.isNull())
            bindings.push_back(value2);
    };

    if (hasStartTimeLimit && hasEndTimeLimit)
    {
        if (isAdditionRangeRequest)
        {
            addFilter("end_time >= ? AND start_time <= ?",
                (qint64) filter.startTimeMs.count(), (qint64) filter.endTimeMs.count());
        }
        else
        {
            addFilter("start_time between ? AND ?",
                (qint64) filter.startTimeMs.count(), (qint64) filter.endTimeMs.count());
        }
    }
    else if (hasEndTimeLimit)
    {
        addFilter("start_time <= ?", (qint64) filter.endTimeMs.count());
    }
    else if (hasStartTimeLimit)
    {
        addFilter("start_time + duration  >= ?", (qint64) filter.startTimeMs.count());
    }

    if (filter.sparsing.minVisibleLengthMs.count() > 0)
        addFilter("duration  >= ?", (qint64) filter.sparsing.minVisibleLengthMs.count());

    if (!filter.text.isEmpty())
    {
        const auto getFilterValue =
            [](const QString& text)
            {
                // The asterisk allows prefix search.
                static const QString filterTemplate = lit("%1*");
                static const QChar delimiter = L' ';

                QStringList result;
                const auto list = text.split(delimiter);
                for (const auto& item: list)
                    result.push_back(filterTemplate.arg(item));

                return result.join(delimiter);
            };

        addFilter(
            "rowid in (SELECT docid FROM fts_bookmarks WHERE fts_bookmarks MATCH ?)", getFilterValue(filter.text));
    }

    QString queryStr = queryTemplate
        .arg(filterText, QString(filter.orderBy.order == Qt::AscendingOrder ? "ASC" : "DESC"));

    if (filter.limit != QnCameraBookmarkSearchFilter::kNoLimit)
        queryStr += lit(" LIMIT %1").arg(filter.limit);

    NX_VERBOSE(this, lit("Got getBookmarks query: %1").arg(queryStr));
    {
        QnWriteLocker lock(&m_mutex);
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
            return false;

        for (const auto& b: bindings)
            query.addBindValue(b);

        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;

        QnSqlIndexMapping mapping = QnSql::mapping<QnCameraBookmark>(query);
        QSqlRecord queryInfo = query.record();
        const int tagsFiledIdx = queryInfo.indexOf("tags");

        while (query.next())
        {
            QnCameraBookmark bookmark;
            QnSql::fetch(mapping, query.record(), &bookmark);
            bookmark.tags = query.value(
                tagsFiledIdx).toString().split(lit(","),
                QString::SkipEmptyParts).toSet();
            result.push_back(std::move(bookmark));
        }
    }

    return true;
}
bool QnServerDb::addBookmark(const QnCameraBookmark& bookmark)
{
    auto result = addOrUpdateBookmark(bookmark, false);
    if (result)
        updateBookmarkCount();

    return result;
}

bool QnServerDb::updateBookmark(const QnCameraBookmark& bookmark)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmarks must not be stored");
    if (!bookmark.isValid())
        return false;

    if (!containsBookmark(bookmark.guid))
        return false;

    return addOrUpdateBookmark(bookmark, true);
}

bool QnServerDb::containsBookmark(const QnUuid& bookmarkId) const
{
    QnWriteLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT guid from bookmarks where guid = ?");
    query.bindValue(0, bookmarkId.toRfc4122());
    return execSQLQuery(&query, Q_FUNC_INFO) && query.next();
}

QnCameraBookmarkTagList QnServerDb::getBookmarkTags(int limit)
{
    QnWriteLocker lock(&m_mutex);

    QString queryStr(R"(
        SELECT tag as name, count
        FROM bookmark_tag_counts
        ORDER BY count DESC
    )");

    if (limit > 0 && limit < std::numeric_limits<int>().max())
        queryStr += lit("LIMIT %1").arg(limit);

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(queryStr);

    QnCameraBookmarkTagList result;

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return result;

    QnSqlIndexMapping mapping = QnSql::mapping<QnCameraBookmarkTag>(query);

    while (query.next())
    {
        QnCameraBookmarkTag tag;
        QnSql::fetch(mapping, query.record(), &tag);
        if (tag.isValid())
            result.append(tag);
    }

    return result;
}

bool QnServerDb::addOrUpdateBookmark(const QnCameraBookmark& bookmark, bool isUpdate)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark must not be stored in database");
    if (!bookmark.isValid())
        return false;

    QnDbTransactionLocker tran(getTransaction());

    int docId = 0;
    if (isUpdate)
    {
        QSqlQuery updQuery(m_sdb);

        static const auto kUpdateQueryText =
            R"(
                UPDATE bookmarks
                SET
                    camera_guid = :cameraId,
                    start_time = :startTimeMs,
                    duration = :durationMs,
                    end_time = :startTimeMs + :durationMs,
                    name = :name,
                    description = :description,
                    timeout = :timeout
                WHERE guid = :guid)";

        updQuery.prepare(kUpdateQueryText);
        QnSql::bind(bookmark, &updQuery);
        if (!execSQLQuery(&updQuery, Q_FUNC_INFO))
            return false;
        QSqlQuery getRowIdQuery(m_sdb);
        getRowIdQuery.prepare("SELECT rowid from bookmarks WHERE guid = :guid");
        getRowIdQuery.addBindValue(QnSql::serialized_field(bookmark.guid));
        if (!execSQLQuery(&getRowIdQuery, Q_FUNC_INFO))
            return false;
        getRowIdQuery.next();
        docId = getRowIdQuery.value(0).toInt();
    }
    else
    {
        QSqlQuery insQuery(m_sdb);
        static const auto kAddQueryText =
            R"(
                INSERT
                INTO bookmarks
                    (guid, camera_guid, start_time, duration, end_time, name, description, timeout,
                        creator_guid, created)
                VALUES (:guid, :cameraId, :startTimeMs, :durationMs, :startTimeMs + :durationMs, :name, :description, :timeout,
                    :creatorId, :creationTimeStampMs)
            )";

        insQuery.prepare(kAddQueryText);
        QnSql::bind(bookmark, &insQuery);
        if (!execSQLQuery(&insQuery, Q_FUNC_INFO))
            return false;

        docId = insQuery.lastInsertId().toInt();
    }

    if (isUpdate)
    {
        QSqlQuery cleanTagQuery(m_sdb);
        cleanTagQuery.prepare("DELETE FROM bookmark_tags WHERE bookmark_guid = ?");
        cleanTagQuery.addBindValue(bookmark.guid.toRfc4122());
        if (!execSQLQuery(&cleanTagQuery, Q_FUNC_INFO))
            return false;
    }

    const auto trimTags =
        [](const QnCameraBookmark &bookmark) -> QnCameraBookmarkTags
        {
            QnCameraBookmarkTags result;
            for (const auto tag: bookmark.tags)
            {
                QString trimmed = tag.trimmed();
                if (!trimmed.isEmpty())
                    result.insert(tag.trimmed());
            }

            return result;
        };

    const auto trimmedTags = trimTags(bookmark);

    {
        QSqlQuery tagQuery(m_sdb);
        tagQuery.prepare(
            "INSERT INTO bookmark_tags (bookmark_guid, name) VALUES (:bookmark_guid, :name)");
        tagQuery.bindValue(":bookmark_guid", bookmark.guid.toRfc4122());
        for (const auto& tag: trimmedTags)
        {
            tagQuery.bindValue(":name", tag);
            if (!execSQLQuery(&tagQuery, Q_FUNC_INFO))
                return false;
        }
    }

    {
        QSqlQuery query(m_sdb);
        const QString insertOrReplace = isUpdate ? lit("REPLACE") : lit("INSERT");
        query.prepare(insertOrReplace + R"(
            INTO fts_bookmarks
                (docid, name, description, tags)
            VALUES (:docid, :name, :description, :tags)
        )");

        query.bindValue(":docid", docId);
        query.bindValue(":name", bookmark.name);
        query.bindValue(":description", bookmark.description);
        query.bindValue(":tags", QnCameraBookmark::tagsToString(trimmedTags));
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return tran.commit();
}

void QnServerDb::updateBookmarkCount()
{
    std::function<void()> finalHandler;

    {
        QnWriteLocker lock(&m_mutex);
        if (!m_updateBookmarkCount)
            return;

        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare(lit("SELECT count(guid) FROM bookmarks"));
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return;

        if (!query.next())
        {
            NX_ASSERT(false, "Query has failed!");
            return;
        }

        const auto value = query.value(0).toString();
        finalHandler = std::bind(m_updateBookmarkCount, value.toInt());
    }
    finalHandler();
}

bool QnServerDb::deleteAllBookmarksForCamera(const QnUuid& cameraId)
{
    bool result;

    {
        QnDbTransactionLocker tran(getTransaction());

        {
            QSqlQuery delQuery(m_sdb);
            delQuery.prepare(R"(
                DELETE FROM bookmark_tags
                WHERE bookmark_guid IN
                    (SELECT guid from bookmarks WHERE camera_guid = :id)
            )");
            delQuery.bindValue(":id", QnSql::serialized_field(cameraId));
            if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
                return false;
        }

        {
            QSqlQuery delQuery(m_sdb);
            delQuery.prepare("DELETE FROM bookmarks WHERE camera_guid = :id");
            delQuery.bindValue(":id", QnSql::serialized_field(cameraId));
            if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
                return false;
        }

        if (!execSQLQuery(R"(
            DELETE FROM fts_bookmarks
            WHERE docid NOT IN (SELECT rowid FROM bookmarks)
        )", m_sdb, Q_FUNC_INFO))
        {
            return false;
        }

        result = tran.commit();
    }

    if (result)
        updateBookmarkCount();

    return result;
}

bool QnServerDb::deleteBookmark(const QnUuid& bookmarkId)
{
    if (!containsBookmark(bookmarkId))
        return false;

    bool result;
    {
        QnDbTransactionLocker tran(getTransaction());

        {
            QSqlQuery cleanTagQuery(m_sdb);
            cleanTagQuery.prepare(
                "DELETE FROM bookmark_tags WHERE bookmark_guid = ?");
            cleanTagQuery.addBindValue(bookmarkId.toRfc4122());
            if (!execSQLQuery(&cleanTagQuery, Q_FUNC_INFO))
                return false;
        }

        {
            QSqlQuery cleanQuery(m_sdb);
            cleanQuery.prepare("DELETE FROM bookmarks WHERE guid = ?");
            cleanQuery.addBindValue(bookmarkId.toRfc4122());
            if (!execSQLQuery(&cleanQuery, Q_FUNC_INFO))
                return false;
        }

        if (!execSQLQuery(
            "DELETE FROM fts_bookmarks WHERE docid NOT IN (SELECT rowid FROM bookmarks)",
            m_sdb, Q_FUNC_INFO))
        {
            return false;
        }

        result = tran.commit();
    }

    if (result)
        updateBookmarkCount();

    return result;
}

void QnServerDb::setBookmarkCountController(std::function<void(size_t)> handler)
{
    {
        QnWriteLocker lock(&m_mutex);
        NX_ASSERT(!m_updateBookmarkCount, "controller is already set");
        m_updateBookmarkCount = std::move(handler);
    }
    updateBookmarkCount();
}

qint64 QnServerDb::getLastRemoteArchiveSyncTimeMs(const QnResourcePtr& resource)
{
    NX_ASSERT(resource, "Resource should be provided");
    if (!resource)
        return false;

    auto id = resource->getId();

    QSqlQuery query(m_sdb);
    query.prepare(R"(
        SELECT property_value
        FROM local_resource_properties
        WHERE resource_id = :resource_id AND property_name = :property_name)");

    query.bindValue(":resource_id", QnSql::serialized_field(id));
    query.bindValue(":property_name", kLastRemoteArchiveSyncTimePropertyName);

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return std::numeric_limits<qint64>::min();

    if (!query.next())
        return std::numeric_limits<qint64>::min();

    bool success = false;
    auto rawVal = query.value(0);

    auto parsed = rawVal.toLongLong(&success);
    if (!success)
        return std::numeric_limits<qint64>::min();

    return parsed;
}

bool QnServerDb::updateLastRemoteArchiveSyncTimeMs(const QnResourcePtr& resource, qint64 lastSyncTime)
{
    NX_ASSERT(resource, "Resource should be provided");
    if (!resource)
        return false;

    auto id = resource->getId();

    QSqlQuery updateQuery(m_sdb);
    updateQuery.prepare(R"(
        INSERT OR REPLACE INTO local_resource_properties
            (id, resource_id, property_name, property_value)
        VALUES
            ((  SELECT id
                FROM local_resource_properties
                WHERE resource_id = :resource_id AND property_name = :property_name),
                :resource_id,
                :property_name,
                :property_value))");

    updateQuery.bindValue(":resource_id", QnSql::serialized_field(id));
    updateQuery.bindValue(":property_name", kLastRemoteArchiveSyncTimePropertyName);
    updateQuery.bindValue(":property_value", QString::number(lastSyncTime));

    return execSQLQuery(&updateQuery, Q_FUNC_INFO);
}

bool QnServerDb::deleteBookmarksToTime(const QMap<QnUuid, qint64>& dataToDelete)
{
    bool result;

    {
        QnDbTransactionLocker tran(getTransaction());

        for (auto itr = dataToDelete.begin(); itr != dataToDelete.end(); ++itr)
        {
            const QnUuid& cameraId = itr.key();
            qint64 timestampMs = itr.value();

            {
                QSqlQuery delQuery(m_sdb);
                delQuery.prepare(R"(
                    DELETE FROM bookmark_tags
                    WHERE bookmark_guid IN
                        (SELECT guid from bookmarks
                            WHERE camera_guid = :id AND start_time < :timestamp)
                )");
                delQuery.bindValue(":id", QnSql::serialized_field(cameraId));
                delQuery.bindValue(":timestamp", timestampMs);
                if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
                    return false;
            }

            {
                QSqlQuery delQuery(m_sdb);
                delQuery.prepare(R"(
                    DELETE FROM bookmarks
                    WHERE camera_guid = :id AND start_time < :timestamp
                )");
                delQuery.bindValue(":id", QnSql::serialized_field(cameraId));
                delQuery.bindValue(":timestamp", timestampMs);
                if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
                    return false;
            }

            if (!execSQLQuery(R"(
                DELETE FROM fts_bookmarks
                WHERE docid NOT IN (SELECT rowid FROM bookmarks)
            )", m_sdb, Q_FUNC_INFO))
            {
                return false;
            }

        }
        result = tran.commit();
    }

    if (result)
        updateBookmarkCount();

    return result;
}
