#include "server_db.h"

#include <QtCore/QtEndian>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/camera_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>

#include <media_server/serverutil.h>

#include <recorder/storage_manager.h>
#include <recording/time_period.h>

#include <media_server/settings.h>

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/fusion/model_functions.h>
#include <api/global_settings.h>

namespace {

const char kDelimiter('$');
const char kStringListDelimiter('\n');

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

QnBusinessActionParameters convertOldActionParameters(const QByteArray& value)
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

    QnBusinessActionParameters result;

    if (value.isEmpty())
        return result;

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
                result.userGroup = static_cast<QnBusiness::UserGroup>(toInt(field));
                break;
            case FpsParam:
                result.fps = toInt(field);
                break;
            case QualityParam:
                result.streamQuality = static_cast<Qn::StreamQuality>(toInt(field));
                break;
            case DurationParam:
                result.recordingDuration = toInt(field);
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

QnBusinessEventParameters convertOldEventParameters(
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

    QnBusinessEventParameters result;

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
                    result.eventType = (QnBusiness::EventType) toInt(field);
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
                    result.reasonCode = (QnBusiness::EventReason) toInt(field);
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

void addGetBookmarksFilter(const QString& text, QString* filter)
{
    if (filter->isEmpty())
        *filter = "WHERE " + text;
    else
        *filter += " AND " + text;
}

void addGetBookmarksFilter(const QString& text, QString* filter, QStringList* bindings)
{
    addGetBookmarksFilter(text, filter);
    bindings->append(text.mid(text.lastIndexOf(':')));
}

QString createBookmarksFilterSortPart(const QnCameraBookmarkSearchFilter& filter)
{
    static const auto kOrderByTemplate = lit(" ORDER BY %1 %2, guid ");

    const auto order = filter.orderBy.order == Qt::AscendingOrder ? lit("ASC") : lit("DESC");
    switch (filter.orderBy.column)
    {
        case Qn::BookmarkName:
            return kOrderByTemplate.arg(lit("book.name"), order);
        case Qn::BookmarkStartTime:
            return kOrderByTemplate.arg(lit("startTimeMs"), order);
        case Qn::BookmarkDuration:
            return kOrderByTemplate.arg(lit("durationMs"), order);
        case Qn::BookmarkCameraName:
            return kOrderByTemplate.arg(lit("cameraId"), order);
        case Qn::BookmarkTags:
            return lit(""); // No sort by db
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Invalid sorting column value");
            return lit("");
    }
}

int getBookmarksQueryLimit(const QnCameraBookmarkSearchFilter &filter)
{
    if (filter.sparsing.used)
        return QnCameraBookmarkSearchFilter::kNoLimit;

    switch (filter.orderBy.column)
    {
        case Qn::BookmarkName:
        case Qn::BookmarkStartTime:
        case Qn::BookmarkDuration:
            return filter.limit;

        case Qn::BookmarkCameraName:
        case Qn::BookmarkTags:
            // No limit for manually sorted sequences.
            return QnCameraBookmarkSearchFilter::kNoLimit;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Invalid sorting column value!");
            return QnCameraBookmarkSearchFilter::kNoLimit;
    }
}

} // namespace

static const qint64 CLEANUP_INTERVAL = 1000000ll * 3600;
static const qint64 DEFAULT_EVENT_KEEP_PERIOD = 1000000ll * 3600 * 24 * 30; //< 30 days

QnServerDb::QnServerDb():
    m_lastCleanuptime(0),
    m_auditCleanuptime(0),
    m_eventKeepPeriod(DEFAULT_EVENT_KEEP_PERIOD),
    m_runtimeActionsTotalRecords(0),
    m_tran(m_sdb, m_mutex)
{
    const QString fileName =
        closeDirPath(
            MSSettings::roSettings()->value("eventsDBFilePath", getDataDirectory()).toString())
        + QString(lit("mserver.sqlite"));
    addDatabase(fileName, "QnServerDb");
    if (m_sdb.open())
    {
        if (!createDatabase()) // Create tables if DB is empty.
            qWarning() << "Cannot create tables for sqlLite database";
        else
            m_runtimeActionsTotalRecords = getRuntimeActionsRecordCount();
    }
    else
    {
        qWarning() << "Cannot initialize sqlLite database. Actions log is not created.";
    }

    if (!execSQLScript("vacuum;", m_sdb))
        qWarning() << "failed to vacuum mserver database" << Q_FUNC_INFO;

    if (!tuneDBAfterOpen(&m_sdb))
        qWarning() << "failed to turn on journal mode for mserver database" << Q_FUNC_INFO;
}

QnServerDb::QnDbTransaction* QnServerDb::getTransaction()
{
    return &m_tran;
}

void QnServerDb::setEventLogPeriod(qint64 periodUsec)
{
    m_eventKeepPeriod = periodUsec;
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

        insQuery.bindValue("id", itr->first);
        QnSql::bind(data, &insQuery);
        if (!execSQLQuery(&insQuery, Q_FUNC_INFO))
            return false;
    }
    cleanupAuditLog();
    tran.commit();
    return true;
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
    request += lit("ORDER BY createdTimeSec");

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
    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    if (currentTime - m_lastCleanuptime > CLEANUP_INTERVAL)
    {
        m_lastCleanuptime = currentTime;
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM runtime_actions where timestamp < :timestamp");
        int utc = (currentTime - m_eventKeepPeriod)/1000000ll;
        delQuery.bindValue(":timestamp", utc);
        rez = execSQLQuery(&delQuery, Q_FUNC_INFO);

        if (rez)
            m_runtimeActionsTotalRecords -= delQuery.numRowsAffected();
    }

    // cleanup by  record count
    const int kMaxRecords = 100000;
    const int kMaxOverflowRecords = 120000;

    if (kMaxOverflowRecords < m_runtimeActionsTotalRecords)
    {
        QSqlQuery cleanupQuery(m_sdb);
        cleanupQuery.prepare(
            "DELETE FROM runtime_actions WHERE rowid in \
            (SELECT rowid FROM runtime_actions ORDER BY rowid LIMIT :recordsToDelete)"
        );
        cleanupQuery.bindValue(":recordsToDelete", m_runtimeActionsTotalRecords - kMaxRecords);
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
            QnBusinessActionParameters ap = convertOldActionParameters(packed);
            ap.actionResourceId = actionResourceId;
            return QnUbjson::serialized(ap);
        };

    auto convertRuntime =
        [](const QByteArray &packed, QnUuid* actionResourceId)-> QByteArray
        {
            // Check if data is in Ubjson already.
            if (!packed.isEmpty() && packed[0] == L'[')
                return packed;
            QnBusinessEventParameters rp = convertOldEventParameters(packed, actionResourceId);
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

    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    if (currentTime - m_auditCleanuptime > CLEANUP_INTERVAL)
    {
        m_auditCleanuptime = currentTime;
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM audit_log where createdTimeSec < :createdTimeSec");
        int utc = currentTime / 1000000ll - qnGlobalSettings->auditTrailPeriodDays() * 3600 * 24;
        delQuery.bindValue(":timestamp", utc);
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

bool QnServerDb::saveActionToDB(const QnAbstractBusinessActionPtr& action)
{
    QnWriteLocker lock(&m_mutex);

    if (action->isReceivedFromRemoteHost())
        return false; //< Server should save the action before proxing locally.

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare(R"(
        INSERT INTO runtime_actions (timestamp, action_type, action_params, runtime_params,
            business_rule_guid, toggle_state, aggregation_count, event_type,
            event_resource_guid, action_resource_guid)
        VALUES (:timestamp, :action_type, :action_params, :runtime_params, :business_rule_guid,
            :toggle_state, :aggregation_count, :event_type, :event_resource_guid, :action_resource_guid);
    )");

    qint64 timestampUsec = action->getRuntimeParams().eventTimestampUsec;
    QnUuid eventResId = action->getRuntimeParams().eventResourceId;

    QnBusinessActionParameters actionParams = action->getParams();

    insQuery.bindValue(":timestamp", timestampUsec/1000000);
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", QnUbjson::serialized(actionParams));
    insQuery.bindValue(":runtime_params", QnUbjson::serialized(action->getRuntimeParams()));
    insQuery.bindValue(":business_rule_guid", action->getBusinessRuleId().toRfc4122());
    insQuery.bindValue(":toggle_state", (int) action->getToggleState());
    insQuery.bindValue(":aggregation_count", action->getAggregationCount());

    insQuery.bindValue(":event_type", (int) action->getRuntimeParams().eventType);
    insQuery.bindValue(":event_resource_guid", eventResId.toRfc4122());
    insQuery.bindValue(":action_resource_guid",
        !actionParams.actionResourceId.isNull()
            ? actionParams.actionResourceId.toRfc4122()
            : QByteArray());

    bool rez = execSQLQuery(&insQuery, Q_FUNC_INFO);
    if (rez)
    {
        m_runtimeActionsTotalRecords += insQuery.numRowsAffected();
        cleanupEvents();
    }

    return rez;
}

QString QnServerDb::getRequestStr(
    const QnTimePeriod& period,
    const QnResourceList& resList,
    const QnBusiness::EventType& eventType,
    const QnBusiness::ActionType& actionType,
    const QnUuid& businessRuleId) const
{
    QString request(lit("SELECT * FROM runtime_actions where"));
    if (!period.isInfinite())
    {
        request += QString(lit(" timestamp between '%1' and '%2'"))
            .arg(period.startTimeMs / 1000).arg(period.endTimeMs() / 1000);
    }
    else
    {
        request += QString(lit(" timestamp >= '%1'")).arg(period.startTimeMs / 1000);
    }

    if (resList.size() == 1)
    {
        request += QString(lit(" and event_resource_guid = %1 "))
            .arg(guidToSqlString(resList[0]->getId()));
    }
    else if (resList.size() > 1)
    {
        QString idList;
        for (const QnResourcePtr& res: resList)
        {
            if (!idList.isEmpty())
                idList += QLatin1Char(',');
            idList += guidToSqlString(res->getId());
        }
        request += QString(lit(" and event_resource_guid in (%1) ")).arg(idList);
    }

    if (eventType != QnBusiness::UndefinedEvent && eventType != QnBusiness::AnyBusinessEvent)
    {
        if (QnBusiness::hasChild(eventType))
        {
            QList<QnBusiness::EventType> events = QnBusiness::childEvents(eventType);
            QString eventTypeStr;
            for(QnBusiness::EventType evnt: events) {
                if (!eventTypeStr.isEmpty())
                    eventTypeStr += QLatin1Char(',');
                eventTypeStr += QString::number((int) evnt);
            }
            request += QString(lit(" and event_type in (%1) ")).arg(eventTypeStr);
        }
        else
        {
            request += QString(lit(" and event_type = %1 ")).arg((int) eventType);
        }
    }
    if (actionType != QnBusiness::UndefinedAction)
        request += QString(lit(" and action_type = %1 ")).arg((int) actionType);
    if (!businessRuleId.isNull())
    {
        request += QString(lit(" and  business_rule_guid = %1 "))
            .arg(guidToSqlString(businessRuleId));
    }

    return request;
}

QnBusinessActionDataList QnServerDb::getActions(
    const QnTimePeriod& period,
    const QnResourceList& resList,
    const QnBusiness::EventType& eventType,
    const QnBusiness::ActionType& actionType,
    const QnUuid& businessRuleId) const
{
    QnBusinessActionDataList result;
    QString request = getRequestStr(period, resList, eventType, actionType, businessRuleId);

    QnWriteLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare(request);
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
        QnBusinessActionData actionData;

        actionData.actionType = (QnBusiness::ActionType) query.value(actionTypeIdx).toInt();
        actionData.actionParams = QnUbjson::deserialized<QnBusinessActionParameters>(
            query.value(actionParamIdx).toByteArray());
        actionData.eventParams = QnUbjson::deserialized<QnBusinessEventParameters>(
            query.value(runtimeParamIdx).toByteArray());
        actionData.businessRuleId = QnUuid::fromRfc4122(
            query.value(businessRuleIdx).toByteArray());
        actionData.aggregationCount = query.value(aggregationCntIdx).toInt();

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

void QnServerDb::getAndSerializeActions(
    QByteArray& result,
    const QnTimePeriod& period,
    const QnResourceList& resList,
    const QnBusiness::EventType& eventType,
    const QnBusiness::ActionType& actionType,
    const QnUuid& businessRuleId) const
{
    QString request = getRequestStr(period, resList, eventType, actionType, businessRuleId);

    QnWriteLocker lock(&m_mutex);

    QSqlQuery actionsQuery(m_sdb);
    actionsQuery.prepare(request);
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
        QnBusiness::EventType eventType =
            (QnBusiness::EventType) actionsQuery.value(eventTypeIdx).toInt();
        if (eventType == QnBusiness::CameraMotionEvent ||
            eventType == QnBusiness::CameraInputEvent)
        {
            QnUuid eventResId = QnUuid::fromRfc4122(actionsQuery.value(eventResIdx).toByteArray());
            QnNetworkResourcePtr camRes =
                qnResPool->getResourceById<QnNetworkResource>(eventResId);
            if (camRes)
            {
                if (QnStorageManager::isArchiveTimeExists(
                    camRes->getUniqueId(), actionsQuery.value(timestampIdx).toInt() * 1000ll))
                {
                    flags |= QnBusinessActionData::VideoLinkExists;
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

    return true;
}

bool QnServerDb::getBookmarks(
    const QnSecurityCamResourceList& cameras,
    const QnCameraBookmarkSearchFilter& filter,
    QnCameraBookmarkList& result)
{
    QSet<QnUuid> cameraIds;
    for (const auto& camera: cameras)
    {
        if (camera && !camera->getId().isNull())
            cameraIds.insert(camera->getId());
    }
    if (cameraIds.empty())
        return false;

    const auto getCameraBindingName =
        [](int index)
        {
            static const auto kBindingTemplate = lit(":cameraUniqueId%1");
            return kBindingTemplate.arg(index);
        };

    QString filterText;

    // Add camera ids to the query.
    QStringList bindings;
    QStringList camerasList;
    int index = 0;
    for (auto it = cameraIds.begin(); it != cameraIds.end(); ++it)
    {
        const auto bindingName = getCameraBindingName(index);
        camerasList.append(lit("book.camera_guid = %1").arg(bindingName));
        bindings.append(bindingName);
        ++index;
    }
    const auto camerasFullFilterTemplate = lit("(%1)").arg(camerasList.join(lit(" OR ")));
    addGetBookmarksFilter(camerasFullFilterTemplate, &filterText);

    if (filter.isValid())
    {
        if (filter.startTimeMs > 0)
            addGetBookmarksFilter("endTimeMs >= :minStartTimeMs", &filterText, &bindings);
        if (filter.endTimeMs < INT64_MAX)
            addGetBookmarksFilter("startTimeMs <= :maxEndTimeMs", &filterText, &bindings);
    }

    if (!filter.text.isEmpty())
    {
        addGetBookmarksFilter(
            "book.rowid in (SELECT docid FROM fts_bookmarks WHERE fts_bookmarks MATCH :text)",
            &filterText);
        // Manual binding: minor hack to work around the closing parenthesis.
        bindings.append(":text");
    }

    const auto limit = getBookmarksQueryLimit(filter);

    QString queryStr = QString(R"(
        SELECT
        book.guid as guid,
        book.name as name,
        book.start_time as startTimeMs,
        book.duration as durationMs,
        book.start_time + book.duration as endTimeMs,
        book.description as description,
        book.timeout as timeout,
        book.camera_guid as cameraId,
        group_concat(tag.name) as tags
        FROM bookmarks book
        LEFT JOIN bookmark_tags tag
        ON book.guid = tag.bookmark_guid
        %1 %2 %3
    )").arg(filterText,
        "GROUP BY guid, startTimeMs, durationMs, endTimeMs, book.name, description, timeout, cameraId",
        createBookmarksFilterSortPart(filter));

    {
        QnWriteLocker lock(&m_mutex);
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
            return false;

        auto checkedBind =
            [&query, &bindings](const QString& placeholder, const QVariant& value)
            {
                if (!bindings.contains(placeholder))
                    return;
                query.bindValue(placeholder, value);
                bindings.removeAll(placeholder);
            };

        index = 0;
        for (auto it = cameraIds.begin(); it != cameraIds.end(); ++it)
        {
            checkedBind(getCameraBindingName(index), QnSql::serialized_field(*it));
            ++index;
        }

        checkedBind(":minStartTimeMs", filter.startTimeMs);
        checkedBind(":maxEndTimeMs", filter.endTimeMs);
        //checkedBind(":minDurationMs", filter.minDurationMs);

        const auto getFilterValue =
            [](const QString& text)
            {
                // The asterisk allows prefix search.
                static const QString filterTemplate = lit("%1*");

                static const QChar delimiter = L' ';

                QStringList result;
                const auto list = text.split(delimiter);
                for(const auto& item: list)
                    result.push_back(filterTemplate.arg(item));

                return result.join(delimiter);
            };

        checkedBind(":text", getFilterValue(filter.text));

        NX_ASSERT(bindings.isEmpty(), Q_FUNC_INFO, "all bindings must be substituted");
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

            if (result.size() > limit) //< We cannot use LIMIT keyword in queries with JOIN.
                break;
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
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmarks must not be stored");
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
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmark must not be stored in database");
    if (!bookmark.isValid())
        return false;

    const QString insertOrReplace = isUpdate ? lit("REPLACE") : lit("INSERT");

    QnDbTransactionLocker tran(getTransaction());

    int docId = 0;
    {
        QSqlQuery insQuery(m_sdb);
        insQuery.prepare(insertOrReplace + R"(
            INTO bookmarks
                (guid, camera_guid, start_time, duration, name, description, timeout)
            VALUES (:guid, :cameraId, :startTimeMs, :durationMs, :name, :description, :timeout)
        )");

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
            NX_ASSERT(false, Q_FUNC_INFO, "Query has failed!");
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
        NX_ASSERT(!m_updateBookmarkCount, Q_FUNC_INFO, "controller is already set");
        m_updateBookmarkCount = std::move(handler);
    }
    updateBookmarkCount();
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
