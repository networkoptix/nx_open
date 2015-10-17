#include "events_db.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QtEndian>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_action_factory.h>

#include <core/resource/network_resource.h>
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
QnEventsDB* QnEventsDB::m_instance = 0;

QnEventsDB::QnEventsDB():
    m_lastCleanuptime(0),
    m_auditCleanuptime(0),
    m_eventKeepPeriod(DEFAULT_EVENT_KEEP_PERIOD),
    m_tran(m_sdb, m_mutex)
{
    m_sdb = QSqlDatabase::addDatabase("QSQLITE");
    m_sdb.setDatabaseName( MSSettings::roSettings()->value( "eventsDBFilePath", closeDirPath(getDataDirectory()) + QString(lit("mserver.sqlite")) ).toString() );
    if (m_sdb.open())
    {
        if (!createDatabase()) // create tables is DB is empty
            qWarning() << "can't create tables for sqlLite database!";
    }
    else {
        qWarning() << "can't initialize sqlLite database! Actions log is not created!";
    }
}

void QnEventsDB::setEventLogPeriod(qint64 periodUsec)
{
    m_eventKeepPeriod = periodUsec;
}

bool QnEventsDB::createDatabase()
{
    QSqlQuery versionQuery(m_sdb);
    versionQuery.prepare("SELECT sql from sqlite_master where name = 'runtime_actions'");
    if (versionQuery.exec() && versionQuery.next())
    {
        QByteArray sql = versionQuery.value("sql").toByteArray();
        versionQuery.clear();
        if (!sql.contains("business_rule_guid")) {
            if (!execSQLQuery("drop index 'timeAndCamIdx'", m_sdb)) {
                return false;
            }
            if (!execSQLQuery("drop table 'runtime_actions'", m_sdb))
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
        if (!ddlQuery.exec())
            return false;
    }

    if (!isObjectExists(lit("index"), lit("timeAndCamIdx"), m_sdb)) {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE INDEX \"timeAndCamIdx\" ON \"runtime_actions\" (timestamp,event_resource_guid)"
        );
        if (!ddlQuery.exec())
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
        if (!ddlQuery.exec())
            return false;
    }

    if (!isObjectExists(lit("index"), lit("auditTimeIdx"), m_sdb)) {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE INDEX \"auditTimeIdx\" ON \"audit_log\" (createdTimeSec)"
            );
        if (!ddlQuery.exec())
            return false;
    }

    if (!isObjectExists(lit("table"), lit("last_backup_time"), m_sdb))
    {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE TABLE \"last_backup_time\" ("
            "camera_id BLOB(16),"
            "catalog INTEGER,"
            "timestamp INTEGER,"
            "primary key(camera_id, catalog, timestamp))"
            );
        if (!ddlQuery.exec())
            return false;
    }

    return true;
}

int QnEventsDB::addAuditRecord(const QnAuditRecord& data)
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

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return -1;
    }
    int result = insQuery.lastInsertId().toInt();
    cleanupAuditLog();
    return result;
}

int QnEventsDB::updateAuditRecord(int internalId, const QnAuditRecord& data)
QnAuditRecordList QnEventsDB::getAuditData(const QnTimePeriod& period, const QnUuid& sessionId)
bool QnEventsDB::cleanupEvents()
bool QnEventsDB::migrateBusinessParams() {
bool QnEventsDB::cleanupAuditLog()
bool QnEventsDB::removeLogForRes(QnUuid resId)
bool QnEventsDB::saveActionToDB(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& actionRes)
QString QnEventsDB::toSQLDate(qint64 timeMs) const
{
    return QDateTime::fromMSecsSinceEpoch(timeMs).toString("yyyy-MM-dd hh:mm:ss");
}

QString QnEventsDB::getRequestStr(const QnTimePeriod& period,
QnBusinessActionDataList QnEventsDB::getActions(
    QElapsedTimer t;
    t.restart();

     int toggleStateIdx = rec.indexOf("toggle_state");
        //actionData.toggleState = (QnBusiness::EventState) query.value(toggleStateIdx).toInt();

    qDebug() << Q_FUNC_INFO << "query time=" << t.elapsed() << "msec";
void QnEventsDB::getAndSerializeActions(
    QElapsedTimer t;
    t.restart();

    QString request = getRequestStr(period, resList, eventType, actionType, businessRuleId);

    QWriteLocker lock(&m_mutex);

    QSqlQuery actionsQuery(m_sdb);
    actionsQuery.prepare(request);
    if (!actionsQuery.exec())
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
                if (QnStorageManager::isArchiveTimeExists(camRes->getUniqueId(), actionsQuery.value(timestampIdx).toInt()*1000ll))
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

    qDebug() << Q_FUNC_INFO << "query time=" << t.elapsed() << "msec";
}

void QnEventsDB::init()
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(!m_instance, Q_FUNC_INFO, "QnEventsDB::init must be called once!");
    m_instance = new QnEventsDB();
}

void QnEventsDB::fini()
{
    delete m_instance;
    m_instance = NULL;
}

QnEventsDB* QnEventsDB::instance()
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(m_instance, Q_FUNC_INFO, "QnEventsDB::init must be called first!");
    return m_instance;
}

bool QnEventsDB::afterInstallUpdate(const QString& updateName) {

    if (updateName == lit(":/mserver_updates/01_business_params.sql")) {
        migrateBusinessParams();
QnEventsDB::QnDbTransaction* QnEventsDB::getTransaction()
{
    return &m_tran;
}
bool QnEventsDB::setLastBackupTime(const QnUuid& cameraId, QnServer::ChunksCatalog catalog, qint64 timestampMs)
{
    QSqlQuery updQuery(m_sdb);
    updQuery.prepare("INSERT OR REPLACE INTO last_backup_time (camera_id, catalog, timestamp) \
                       VALUES (:camera_id, :catalog, :timestamp)");
    updQuery.addBindValue(QnSql::serialized_field(cameraId));
    updQuery.addBindValue((int) catalog);
    updQuery.addBindValue(timestampMs);
    bool result = updQuery.exec();
    if (!result)
        qWarning() << Q_FUNC_INFO << updQuery.lastError().text();
    return result;
}

qint64 QnEventsDB::getLastBackupTime(const QnUuid& cameraId, QnServer::ChunksCatalog catalog)
{
    qint64 result = 0;

    QSqlQuery query(m_sdb);
    query.prepare("SELECT timestamp FROM last_backup_time WHERE camera_id = :camera_id AND catalog = :catalog");
    query.addBindValue(QnSql::serialized_field(cameraId));
    query.addBindValue((int) catalog);
    if (query.exec()) {
        if (query.next())
            result = query.value(0).toLongLong();
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
    }

    return result;
