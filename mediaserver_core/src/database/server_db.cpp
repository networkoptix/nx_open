#include "server_db.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QtEndian>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_action_factory.h>

#include <core/resource/network_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>

#include <media_server/serverutil.h>

#include <recorder/storage_manager.h>
#include <recording/time_period.h>

#include <media_server/settings.h>

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/common/model_functions.h>

namespace {

    const char DELIMITER('$');
    const char STRING_LIST_DELIM('\n');

    inline int toInt(const QByteArray& ba)
    {
        const char* curPtr = ba.data();
        const char* end = curPtr + ba.size();
        int result = 0;
        for(; curPtr < end; ++curPtr)
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
        for(; curPtr < end; ++curPtr)
        {
            if (*curPtr < '0' || *curPtr > '9')
                return result;
            result = result*10 + (*curPtr - '0');
        }
        return result;
    }

    QnBusinessActionParameters convertOldActionParameters(const QByteArray &value) {
        enum Param {
            SoundUrlParam,
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
            int nextPos = value.indexOf(DELIMITER, prevPos+1);
            if (nextPos == -1)
                nextPos = value.size();

            QByteArray field(value.data() + prevPos + 1, nextPos - prevPos - 1);
            switch ((Param) i)
            {
            case SoundUrlParam:
                result.soundUrl = QString::fromUtf8(field.data(), field.size());
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
                result.relayAutoResetTimeout = toInt(field);
                break;
            case InputPortIdParam:
                result.inputPortId = QString::fromUtf8(field.data(), field.size());
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

    QnBusinessEventParameters convertOldEventParameters(const QByteArray& value, QnUuid* actionResourceId) {
        enum Param {
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
            int nextPos = value.indexOf(DELIMITER, prevPos+1);
            if (nextPos == -1)
                nextPos = value.size();

            QByteArray field = QByteArray::fromRawData(value.data() + prevPos + 1, nextPos - prevPos - 1);
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
}

static const qint64 CLEANUP_INTERVAL = 1000000ll * 3600;
static const qint64 DEFAULT_EVENT_KEEP_PERIOD = 1000000ll * 3600 * 24 * 30; // 30 days

QnServerDb::QnServerDb():
    m_lastCleanuptime(0),
    m_auditCleanuptime(0),
    m_eventKeepPeriod(DEFAULT_EVENT_KEEP_PERIOD),
    m_tran(m_sdb, m_mutex)
{
    const QString dbName = closeDirPath(MSSettings::roSettings()->value( "eventsDBFilePath", getDataDirectory()).toString())
        + QString(lit("mserver.sqlite"));
    m_sdb = QSqlDatabase::addDatabase("QSQLITE");
    m_sdb.setDatabaseName(dbName);
    if (m_sdb.open())
    {
        if (!createDatabase()) // create tables is DB is empty
            qWarning() << "can't create tables for sqlLite database!";
    }
    else {
        qWarning() << "can't initialize sqlLite database! Actions log is not created!";
    }
}

QnServerDb::QnDbTransaction* QnServerDb::getTransaction() {
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
        if (!sql.contains("business_rule_guid")) {
            if (!execSQLQuery("drop index 'timeAndCamIdx'", m_sdb, Q_FUNC_INFO)) {
                return false;
            }
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

    if (!isObjectExists(lit("index"), lit("timeAndCamIdx"), m_sdb)) {
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

    if (!isObjectExists(lit("index"), lit("auditTimeIdx"), m_sdb)) {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE INDEX \"auditTimeIdx\" ON \"audit_log\" (createdTimeSec)"
            );
        if (!execSQLQuery(&ddlQuery, Q_FUNC_INFO))
            return false;
    }

    if( !tran.commit() ) {
        qWarning() << Q_FUNC_INFO << m_sdb.lastError().text();
        return false;
    }

    return true;
}

int QnServerDb::addAuditRecord(const QnAuditRecord& data)
{
    QWriteLocker lock(&m_mutex);

    Q_ASSERT(data.eventType != Qn::AR_NotDefined);
    Q_ASSERT((data.eventType & (data.eventType-1)) == 0);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO audit_log"
        "(createdTimeSec, rangeStartSec, rangeEndSec, eventType, resources, params, authSession)"
        "VALUES"
        "(:createdTimeSec, :rangeStartSec, :rangeEndSec, :eventType, :resources, :params, :authSession)"
        );
    QnSql::bind(data, &insQuery);

    if (!execSQLQuery(&insQuery, Q_FUNC_INFO))
        return -1;
    
    int result = insQuery.lastInsertId().toInt();
    cleanupAuditLog();
    return result;
}

int QnServerDb::updateAuditRecord(int internalId, const QnAuditRecord& data)
{
    QWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;
    Q_ASSERT(data.eventType != Qn::AR_NotDefined);
    Q_ASSERT((data.eventType & (data.eventType-1)) == 0);

    QSqlQuery updQuery(m_sdb);
    updQuery.prepare("UPDATE audit_log SET "
        "createdTimeSec = :createdTimeSec, rangeStartSec = :rangeStartSec, rangeEndSec = :rangeEndSec, eventType = :eventType, resources = :resources, "
        "params = :params, authSession = :authSession WHERE id = :id"
        );
    QnSql::bind(data, &updQuery);
    updQuery.bindValue(":id", internalId);

    if (!execSQLQuery(&updQuery, Q_FUNC_INFO))
        return -1;
    
    return internalId;
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

        

    QWriteLocker lock(&m_mutex);
    QSqlQuery query(m_sdb);
    query.prepare(request);
    query.addBindValue(period.startTimeMs / 1000);
    query.addBindValue(period.endTimeMs() == DATETIME_NOW ? INT_MAX : period.endTimeMs() / 1000);
    
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return result;
    
    QnSql::fetch_many(query, &result);

    return result;
}

bool QnServerDb::cleanupEvents()
{
    bool rez = true;

    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    if (currentTime - m_lastCleanuptime > CLEANUP_INTERVAL)
    {
        m_lastCleanuptime = currentTime;
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM runtime_actions where timestamp < :timestamp");
        int utc = (currentTime - m_eventKeepPeriod)/1000000ll;
        delQuery.bindValue(":timestamp", utc);
        rez = execSQLQuery(&delQuery, Q_FUNC_INFO);
    }
    return rez;
}

bool QnServerDb::migrateBusinessParams() {
    struct RowParams {
        QByteArray actionParams;
        QByteArray runtimeParams;
    };

    auto convertAction = [](const QByteArray &packed, const QnUuid& actionResourceId) -> QByteArray {
        /* Check if data is in Ubjson already. */
        if (!packed.isEmpty() && packed[0] == L'[')
            return packed;
        QnBusinessActionParameters ap = convertOldActionParameters(packed);
        ap.actionResourceId = actionResourceId;
        return QnUbjson::serialized(ap);
    };

    auto convertRuntime = [](const QByteArray &packed, QnUuid* actionResourceId)-> QByteArray {
        /* Check if data is in Ubjson already. */
        if (!packed.isEmpty() && packed[0] == L'[')
            return packed;
        QnBusinessEventParameters rp = convertOldEventParameters(packed, actionResourceId);
        return QnUbjson::serialized(rp);
    };

    QMap<int, RowParams> remapData;

    { /* Reading data from the table. */
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare(QString("SELECT rowid, action_params, runtime_params FROM runtime_actions order by rowid"));
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;

        while (query.next()) {
            qint32 id = query.value(0).toInt();
            QByteArray actionData = query.value(1).toByteArray();
            QByteArray runtimeData = query.value(2).toByteArray();

            RowParams remappedData;
            QnUuid actionResourceId;
            remappedData.runtimeParams = convertRuntime(runtimeData, &actionResourceId); // move actionResourceId from runtimeParams to actionParams
            remappedData.actionParams = convertAction(actionData, actionResourceId);
            remapData[id] = remappedData;
        }
    }
    

    {  
        QnDbTransactionLocker tran(getTransaction());

        QSqlQuery query(m_sdb);
        query.prepare("UPDATE runtime_actions SET action_params = :action, runtime_params = :runtime WHERE rowid = :rowid");
        for(auto iter = remapData.cbegin(); iter != remapData.cend(); ++iter) {
            query.bindValue(":rowid", iter.key());
            query.bindValue(":action", iter->actionParams);
            query.bindValue(":runtime", iter->runtimeParams);
            if (!execSQLQuery(&query, Q_FUNC_INFO))
                return false;
        }

        return tran.commit();
    }
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
        int utc = (currentTime - m_eventKeepPeriod)/1000000ll;
        delQuery.bindValue(":timestamp", utc);
        rez = execSQLQuery(&delQuery, Q_FUNC_INFO);
    }
    return rez;
}

bool QnServerDb::removeLogForRes(QnUuid resId)
{
    QWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM runtime_actions where event_resource_guid = :id1 or action_resource_guid = :id2");


    delQuery.bindValue(":id1", resId.toRfc4122());
    delQuery.bindValue(":id2", resId.toRfc4122());

    return execSQLQuery(&delQuery, Q_FUNC_INFO);
}

bool QnServerDb::saveActionToDB(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& actionRes)
{
    QWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO runtime_actions (timestamp, action_type, action_params, runtime_params, business_rule_guid, toggle_state, aggregation_count, event_type, "
        "event_resource_guid, action_resource_guid) "
        "VALUES (:timestamp, :action_type, :action_params, :runtime_params, :business_rule_guid, :toggle_state, :aggregation_count, :event_type, :event_resource_guid, :action_resource_guid);");

    qint64 timestampUsec = action->getRuntimeParams().eventTimestampUsec;
    QnUuid eventResId = action->getRuntimeParams().eventResourceId;
    
    QnBusinessActionParameters actionParams = action->getParams();
    if (actionRes)
        actionParams.actionResourceId = actionRes->getId();

    insQuery.bindValue(":timestamp", timestampUsec/1000000);
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", QnUbjson::serialized(actionParams));
    insQuery.bindValue(":runtime_params", QnUbjson::serialized(action->getRuntimeParams()));
    insQuery.bindValue(":business_rule_guid", action->getBusinessRuleId().toRfc4122());
    insQuery.bindValue(":toggle_state", (int) action->getToggleState());
    insQuery.bindValue(":aggregation_count", action->getAggregationCount());

    insQuery.bindValue(":event_type", (int) action->getRuntimeParams().eventType);
    insQuery.bindValue(":event_resource_guid", eventResId.toRfc4122());
    insQuery.bindValue(":action_resource_guid", actionRes ? actionRes->getId().toRfc4122() : QByteArray());

    bool rez = execSQLQuery(&insQuery, Q_FUNC_INFO);
    if (rez)
        cleanupEvents();
    return rez;
}

QString QnServerDb::getRequestStr(const QnTimePeriod& period,
                                  const QnResourceList& resList,
                                  const QnBusiness::EventType& eventType, 
                                  const QnBusiness::ActionType& actionType,
                                  const QnUuid& businessRuleId) const

{
    QString request(lit("SELECT * FROM runtime_actions where"));
    if (!period.isInfinite()) {
        request += QString(lit(" timestamp between '%1' and '%2'")).arg(period.startTimeMs/1000).arg(period.endTimeMs()/1000);
    }
    else {
        request += QString(lit(" timestamp >= '%1'")).arg(period.startTimeMs/1000);
    }

    if (resList.size() == 1)
        request += QString(lit(" and event_resource_guid = %1 ")).arg(guidToSqlString(resList[0]->getId()));
    else if (resList.size() > 1) {
        QString idList;
        for(const QnResourcePtr& res: resList) {
            if (!idList.isEmpty())
                idList += QLatin1Char(',');
            idList += guidToSqlString(res->getId());
        }
        request += QString(lit(" and event_resource_guid in (%1) ")).arg(idList);
    }

    if (eventType != QnBusiness::UndefinedEvent && eventType != QnBusiness::AnyBusinessEvent)
    {
        if (QnBusiness::hasChild(eventType)) {
            QList<QnBusiness::EventType> events = QnBusiness::childEvents(eventType);
            QString eventTypeStr;
            for(QnBusiness::EventType evnt: events) {
                if (!eventTypeStr.isEmpty())
                    eventTypeStr += QLatin1Char(',');
                eventTypeStr += QString::number((int) evnt);
            }
            request += QString(lit( " and event_type in (%1) ")).arg(eventTypeStr);
        }
        else {
            request += QString(lit( " and event_type = %1 ")).arg((int) eventType);
        }
    }
    if (actionType != QnBusiness::UndefinedAction)
        request += QString(lit( " and action_type = %1 ")).arg((int) actionType);
    if (!businessRuleId.isNull())
        request += QString(lit( " and  business_rule_guid = %1 ")).arg(guidToSqlString(businessRuleId));

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

    QWriteLocker lock(&m_mutex);

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
        actionData.actionParams = QnUbjson::deserialized<QnBusinessActionParameters>(query.value(actionParamIdx).toByteArray());
        actionData.eventParams = QnUbjson::deserialized<QnBusinessEventParameters>(query.value(runtimeParamIdx).toByteArray());
        actionData.businessRuleId = QnUuid::fromRfc4122(query.value(businessRuleIdx).toByteArray());
        actionData.aggregationCount = query.value(aggregationCntIdx).toInt();

        result.push_back(std::move(actionData));
    }

    return result;
}

inline void appendIntToBA(QByteArray& ba, int value)
{
    value = qToBigEndian(value);
    ba.append((const char*) &value, sizeof(int));
}

inline void appendQnIdToBA(QByteArray& ba, const QnUuid& value)
{
    ba.append(value.toRfc4122());
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

    QWriteLocker lock(&m_mutex);

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
        QnBusiness::EventType eventType = (QnBusiness::EventType) actionsQuery.value(eventTypeIdx).toInt();
        if (eventType == QnBusiness::CameraMotionEvent) 
        {
            QnUuid eventResId = QnUuid::fromRfc4122(actionsQuery.value(eventResIdx).toByteArray());
            QnNetworkResourcePtr camRes = qnResPool->getResourceById<QnNetworkResource>(eventResId);
            if (camRes) {
                if (qnStorageMan->isArchiveTimeExists(camRes->getUniqueId(), actionsQuery.value(timestampIdx).toInt()*1000ll))
                    flags |= QnBusinessActionData::MotionExists;

            }
        }

        appendIntToBA(result, flags);
        appendIntToBA(result, actionsQuery.value(actionTypeIdx).toInt());
        result.append(actionsQuery.value(businessRuleIdx).toByteArray());
        appendIntToBA(result, actionsQuery.value(aggregationCntIdx).toInt());

        QByteArray runtimeParams = actionsQuery.value(runtimeParamIdx).toByteArray();
        appendIntToBA(result, runtimeParams.size());
        result.append(runtimeParams);

        QByteArray actionParams = actionsQuery.value(actionParamIdx).toByteArray();
        appendIntToBA(result, actionParams.size());
        result.append(actionParams);

        ++sizeField;
    }
    sizeField = qToBigEndian(sizeField);
    memcpy(result.data(), &sizeField, sizeof(int));
}

bool QnServerDb::afterInstallUpdate(const QString& updateName) {

    if (updateName == lit(":/mserver_updates/01_business_params.sql")) {
        migrateBusinessParams();
    }

    return true;
}

bool QnServerDb::getBookmarks(const QString& cameraUniqueId, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result) {
    

    QString filterStr;
    QStringList bindings;

    auto addFilter = [&filterStr, &bindings](const QString &text) {
        if (filterStr.isEmpty())
            filterStr = "WHERE " + text;
        else
            filterStr = filterStr + " AND " + text;
        bindings.append(text.mid(text.lastIndexOf(':')));
    };

    if (!cameraUniqueId.isEmpty())
        addFilter("book.unique_id = :cameraUniqueId");

    if (filter.isValid()) {
        if (filter.startTimeMs > 0)
            addFilter("endTimeMs >= :minStartTimeMs");
        if (filter.endTimeMs < INT64_MAX)
            addFilter("startTimeMs <= :maxEndTimeMs");
    }
    //     if (filter.minDurationMs > 0)
    //         addFilter("durationMs >= :minDurationMs");
    //TODO: #GDM #Bookmarks add strategy filter
    if (!filter.text.isEmpty()) {
        addFilter("book.rowid in (SELECT docid FROM fts_bookmarks WHERE fts_bookmarks MATCH :text)");
        bindings.append(":text");   //minor hack to workaround closing bracket
    }

    QString queryStr("SELECT \
                     book.guid as guid, \
                     book.start_time as startTimeMs, \
                     book.duration as durationMs, \
                     book.start_time + book.duration as endTimeMs, \
                     book.name as name, \
                     book.description as description, \
                     book.timeout as timeout, \
                     book.unique_id as cameraId, \
                     tag.name as tagName \
                     FROM storage_bookmark book \
                     LEFT JOIN storage_bookmark_tag tag \
                     ON book.guid = tag.bookmark_guid " 
                     + filterStr +
                     " ORDER BY startTimeMs ASC, guid");

    {
        QWriteLocker lock(&m_mutex);
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare(queryStr);

        auto checkedBind = [&query, &bindings](const QString &placeholder, const QVariant &value) {
            if (!bindings.contains(placeholder))
                return;
            query.bindValue(placeholder, value);
        };

        checkedBind(":cameraUniqueId", cameraUniqueId);
        checkedBind(":minStartTimeMs", filter.startTimeMs);
        checkedBind(":maxEndTimeMs", filter.endTimeMs);
        //checkedBind(":minDurationMs", filter.minDurationMs);
        checkedBind(":text", filter.text + lit("*")); // The star symbol allows prefix search

        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;

        QnSqlIndexMapping mapping = QnSql::mapping<QnCameraBookmark>(query);

        QSqlRecord queryInfo = query.record();
        int guidFieldIdx = queryInfo.indexOf("guid");
        int tagNameFieldIdx = queryInfo.indexOf("tagName");

        QnUuid prevGuid;

        while (query.next()) {
            QnUuid guid = QnUuid::fromRfc4122(query.value(guidFieldIdx).toByteArray());
            if (guid != prevGuid) {
                prevGuid = guid;
                result.push_back(QnCameraBookmark());
                QnSql::fetch(mapping, query.record(), &result.back());
            }

            QString tag = query.value(tagNameFieldIdx).toString();
            if (!tag.isEmpty())
                result.back().tags.insert(tag);
        }
    }
    return true;
}

bool QnServerDb::addBookmark(const QnCameraBookmark &bookmark) {
    return addOrUpdateBookmark(bookmark);
}

bool QnServerDb::updateBookmark(const QnCameraBookmark &bookmark) {
    if (!containsBookmark(bookmark.guid))
        return false;
    return addOrUpdateBookmark(bookmark);
}

bool QnServerDb::containsBookmark(const QnUuid &bookmarkId) const {
    QWriteLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT guid from storage_bookmark where guid = ?");
    query.bindValue(0, bookmarkId.toRfc4122());
    return (execSQLQuery(&query, Q_FUNC_INFO) && query.next());
}


bool QnServerDb::addOrUpdateBookmark( const QnCameraBookmark &bookmark) {
    Q_ASSERT_X(!bookmark.cameraId.isNull(), Q_FUNC_INFO, "Empty bookmark camera id");

    QnDbTransactionLocker tran(getTransaction());

    int docId = 0;
    {
        QSqlQuery insQuery(m_sdb);
        insQuery.prepare("INSERT OR REPLACE INTO storage_bookmark ( \
                         guid, unique_id, start_time, duration, \
                         name, description, timeout \
                         ) VALUES ( \
                         :guid, :cameraId, :startTimeMs, :durationMs, \
                         :name, :description, :timeout \
                         )");
        QnSql::bind(bookmark, &insQuery);
        if (!execSQLQuery(&insQuery, Q_FUNC_INFO))
            return false;

        docId = insQuery.lastInsertId().toInt();
    }

    {
        QSqlQuery cleanTagQuery(m_sdb);
        cleanTagQuery.prepare("DELETE FROM storage_bookmark_tag WHERE bookmark_guid = ?");
        cleanTagQuery.addBindValue(bookmark.guid.toRfc4122());
        if (!execSQLQuery(&cleanTagQuery, Q_FUNC_INFO))
            return false;
    }

    {
        QSqlQuery tagQuery(m_sdb);
        tagQuery.prepare("INSERT INTO storage_bookmark_tag ( bookmark_guid, name ) VALUES ( :bookmark_guid, :name )");
        tagQuery.bindValue(":bookmark_guid", bookmark.guid.toRfc4122());
        for (const QString tag: bookmark.tags) {
            tagQuery.bindValue(":name", tag);
            if (!execSQLQuery(&tagQuery, Q_FUNC_INFO))
                return false;
        }
    }

    {
        QSqlQuery query(m_sdb);
        query.prepare("INSERT OR REPLACE INTO fts_bookmarks (docid, name, description, tags) VALUES ( :docid, :name, :description, :tags )");
        query.bindValue(":docid", docId);
        query.bindValue(":name", bookmark.name);
        query.bindValue(":description", bookmark.description);
        query.bindValue(":tags", bookmark.tagsAsString(L' '));
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return tran.commit();
}

bool QnServerDb::deleteAllBookmarksForCamera(const QString& cameraUniqueId) {
    QnDbTransactionLocker tran(getTransaction());

    {
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM storage_bookmark_tag WHERE bookmark_guid IN (SELECT guid from storage_bookmark WHERE unique_id = :id)");
        delQuery.bindValue(":id", cameraUniqueId);
        if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
            return false;
    }

    {
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM storage_bookmark WHERE unique_id = :id");
        delQuery.bindValue(":id", cameraUniqueId);
        if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
            return false;
    }

    if (!execSQLQuery("DELETE FROM fts_bookmarks WHERE docid NOT IN (SELECT rowid FROM storage_bookmark)", m_sdb, Q_FUNC_INFO))
        return false;

    return tran.commit();
}

bool QnServerDb::deleteBookmark(const QnUuid &bookmarkId) {
    if (!containsBookmark(bookmarkId))
        return false;

    QnDbTransactionLocker tran(getTransaction());

    {
        QSqlQuery cleanTagQuery(m_sdb);
        cleanTagQuery.prepare("DELETE FROM storage_bookmark_tag WHERE bookmark_guid = ?");
        cleanTagQuery.addBindValue(bookmarkId.toRfc4122());
        if (!execSQLQuery(&cleanTagQuery, Q_FUNC_INFO))
            return false;
    }

    {
        QSqlQuery cleanQuery(m_sdb);
        cleanQuery.prepare("DELETE FROM storage_bookmark WHERE guid = ?");
        cleanQuery.addBindValue(bookmarkId.toRfc4122());
        if (!execSQLQuery(&cleanQuery, Q_FUNC_INFO))
            return false;
    }

    if (!execSQLQuery("DELETE FROM fts_bookmarks WHERE docid NOT IN (SELECT rowid FROM storage_bookmark)", m_sdb, Q_FUNC_INFO))
        return false;

    return tran.commit();
}
