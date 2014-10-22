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

    QnBusinessEventParameters convertOldEventParameters(const QByteArray& value) {
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
                    result.eventTimestamp = toInt64(field);
                    break;
                case EventResourceParam:
                    result.eventResourceId = QnUuid(field);
                    break;
                case ActionResourceParam:
                    result.actionResourceId = QnUuid(field);
                    break;
                case InputPortIdParam:
                    result.inputPortId = QString::fromUtf8(field.data(), field.size());
                    break;
                case ReasonCodeParam:
                    result.reasonCode = (QnBusiness::EventReason) toInt(field);
                    break;
                case ReasonParamsEncodedParam:
                    result.reasonParamsEncoded = QString::fromUtf8(field.data(), field.size());
                    break;
                case SourceParam:
                    result.source = QString::fromUtf8(field.data(), field.size());
                    break;
                case ConflictsParam:
                    result.conflicts = QString::fromLatin1(field.data(), field.size()).split(QLatin1Char(STRING_LIST_DELIM)); // optimization. mac address list here. UTF is not required
                    break;
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

static const qint64 EVENTS_CLEANUP_INTERVAL = 1000000ll * 3600;
static const qint64 DEFAULT_EVENT_KEEP_PERIOD = 1000000ll * 3600 * 24 * 30; // 30 days
QnEventsDB* QnEventsDB::m_instance = 0;

QnEventsDB::QnEventsDB():
    m_lastCleanuptime(0),
    m_eventKeepPeriod(DEFAULT_EVENT_KEEP_PERIOD)
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

    if (!isObjectExists(lit("table"), lit("south_migrationhistory"), m_sdb)) {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(
            "CREATE TABLE \"south_migrationhistory\" (\
            \"id\" integer NOT NULL PRIMARY KEY autoincrement,\
            \"app_name\" varchar(255) NOT NULL,\
            \"migration\" varchar(255) NOT NULL,\
            \"applied\" datetime NOT NULL);"
            );
        if (!ddlQuery.exec())
            return false;
    }

    if (!applyUpdates(":/mserver_updates"))
        return false;

    return true;
}

bool QnEventsDB::cleanupEvents()
{
    bool rez = true;

    qint64 currentTime = qnSyncTime->currentUSecsSinceEpoch();
    if (currentTime - m_lastCleanuptime > EVENTS_CLEANUP_INTERVAL)
    {
        m_lastCleanuptime = currentTime;
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM runtime_actions where timestamp < :timestamp;");
        int utc = (currentTime - m_eventKeepPeriod)/1000000ll;
        delQuery.bindValue(":timestamp", utc);
        rez = delQuery.exec();
    }
    return rez;
}

bool QnEventsDB::migrateBusinessParams() {
    struct RowParams {
        QByteArray actionParams;
        QByteArray runtimeParams;
    };

    auto convertAction = [](const QByteArray &packed) {
        QnBusinessActionParameters ap = convertOldActionParameters(packed);
        return QnUbjson::serialized(ap);
    };

    auto convertRuntime = [](const QByteArray &packed) {
        QnBusinessEventParameters rp = convertOldEventParameters(packed);
        return QnUbjson::serialized(rp);
    };

    QMap<int, RowParams> remapData;

    { /* Reading data from the table. */
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare(QString("SELECT rowid, action_params, runtime_params FROM runtime_actions order by rowid"));
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }

        while (query.next()) {
            qint32 id = query.value(0).toInt();
            QByteArray actionData = query.value(1).toByteArray();
            QByteArray runtimeData = query.value(2).toByteArray();

            RowParams remappedData;
            remappedData.actionParams = convertAction(actionData);
            remappedData.runtimeParams = convertRuntime(runtimeData);
            remapData[id] = remappedData;
        }
    }


    for(auto iter = remapData.cbegin(); iter != remapData.cend(); ++iter) {
        QSqlQuery query(m_sdb);
        query.prepare("UPDATE runtime_actions SET action_params = :action, runtime_params = :runtime WHERE rowid = :rowid");
        query.bindValue(":rowid", iter.key());
        query.bindValue(":action", iter->actionParams);
        query.bindValue(":runtime", iter->runtimeParams);
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;
}


bool QnEventsDB::removeLogForRes(QnUuid resId)
{
    QWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM runtime_actions where event_resource_guid = :id1 or action_resource_guid = :id2");


    delQuery.bindValue(":id1", resId.toRfc4122());
    delQuery.bindValue(":id2", resId.toRfc4122());

    return delQuery.exec();
}

bool QnEventsDB::saveActionToDB(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& actionRes)
{
    QWriteLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO runtime_actions (timestamp, action_type, action_params, runtime_params, business_rule_guid, toggle_state, aggregation_count, event_type, "
        "event_resource_guid, action_resource_guid) "
        "VALUES (:timestamp, :action_type, :action_params, :runtime_params, :business_rule_guid, :toggle_state, :aggregation_count, :event_type, :event_resource_guid, :action_resource_guid);");

    qint64 timestampUsec = action->getRuntimeParams().eventTimestamp;
    QnUuid eventResId = action->getRuntimeParams().eventResourceId;
    
    QnBusinessEventParameters actionRuntime = action->getRuntimeParams();
    if (actionRes)
        actionRuntime.actionResourceId = actionRes->getId();
    int eventType = (int) actionRuntime.eventType;

    insQuery.bindValue(":timestamp", timestampUsec/1000000);
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", QnUbjson::serialized(action->getParams()));
    insQuery.bindValue(":runtime_params", QnUbjson::serialized(actionRuntime));
    insQuery.bindValue(":business_rule_guid", action->getBusinessRuleId().toRfc4122());
    insQuery.bindValue(":toggle_state", (int) action->getToggleState());
    insQuery.bindValue(":aggregation_count", action->getAggregationCount());

    insQuery.bindValue(":event_type", eventType);
    insQuery.bindValue(":event_resource_guid", eventResId.toRfc4122());
    insQuery.bindValue(":action_resource_guid", actionRes ? actionRes->getId().toRfc4122() : QByteArray());

    bool rez = insQuery.exec();
    if (rez)
        cleanupEvents();
    return rez;
}

QString QnEventsDB::toSQLDate(qint64 timeMs) const
{
    return QDateTime::fromMSecsSinceEpoch(timeMs).toString("yyyy-MM-dd hh:mm:ss");
}

QString QnEventsDB::getRequestStr(const QnTimePeriod& period,
                                  const QnResourceList& resList,
                                  const QnBusiness::EventType& eventType, 
                                  const QnBusiness::ActionType& actionType,
                                  const QnUuid& businessRuleId) const

{
    QString request(lit("SELECT * FROM runtime_actions where"));
    if (period.durationMs != -1) {
        request += QString(lit(" timestamp between '%1' and '%2'")).arg(period.startTimeMs/1000).arg(period.endTimeMs()/1000);
    }
    else {
        request += QString(lit(" timestamp >= '%1'")).arg(period.startTimeMs/1000);
    }

    if (resList.size() == 1)
        request += QString(lit(" and event_resource_guid = %1 ")).arg(guidToSqlString(resList[0]->getId()));
    else if (resList.size() > 1) {
        QString idList;
        foreach(const QnResourcePtr& res, resList) {
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
            foreach(QnBusiness::EventType evnt, events) {
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

QList<QnAbstractBusinessActionPtr> QnEventsDB::getActions(
    const QnTimePeriod& period,
    const QnResourceList& resList, 
    const QnBusiness::EventType& eventType, 
    const QnBusiness::ActionType& actionType,
    const QnUuid& businessRuleId) const

{
    QElapsedTimer t;
    t.restart();

    QList<QnAbstractBusinessActionPtr> result;
    QString request = getRequestStr(period, resList, eventType, actionType, businessRuleId);

    QReadLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare(request);
    if (!query.exec())
        return result;

     QSqlRecord rec = query.record();
     int actionTypeIdx = rec.indexOf("action_type");
     int actionParamIdx = rec.indexOf("action_params");
     int runtimeParamIdx = rec.indexOf("runtime_params");
     int businessRuleIdx = rec.indexOf("business_rule_guid");
     int toggleStateIdx = rec.indexOf("toggle_state");
     int aggregationCntIdx = rec.indexOf("aggregation_count");

    while (query.next()) 
    {
        QnBusiness::ActionType actionType = (QnBusiness::ActionType) query.value(actionTypeIdx).toInt();
        QnBusinessActionParameters actionParams = QnUbjson::deserialized<QnBusinessActionParameters>(query.value(actionParamIdx).toByteArray());
        QnBusinessEventParameters runtimeParams = QnUbjson::deserialized<QnBusinessEventParameters>(query.value(runtimeParamIdx).toByteArray());
        QnAbstractBusinessActionPtr action = QnBusinessActionFactory::createAction(actionType, runtimeParams);
        action->setParams(actionParams);
        action->setBusinessRuleId(QnUuid(query.value(businessRuleIdx).toByteArray()));
        action->setToggleState( (QnBusiness::EventState) query.value(toggleStateIdx).toInt());
        action->setAggregationCount(query.value(aggregationCntIdx).toInt());

        result << action;
    }

    qDebug() << Q_FUNC_INFO << "query time=" << t.elapsed() << "msec";

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

void QnEventsDB::getAndSerializeActions(
                                        QByteArray& result,
                                        const QnTimePeriod& period,
                                        const QnResourceList& resList,
                                        const QnBusiness::EventType& eventType, 
                                        const QnBusiness::ActionType& actionType,
                                        const QnUuid& businessRuleId) const

{
    QElapsedTimer t;
    t.restart();

    QString request = getRequestStr(period, resList, eventType, actionType, businessRuleId);

    QReadLocker lock(&m_mutex);

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
            QnNetworkResourcePtr camRes = qnResPool->getResourceById(eventResId).dynamicCast<QnNetworkResource>();
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
    }

    return true;
}
