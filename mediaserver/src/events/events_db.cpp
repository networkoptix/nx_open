#include "events_db.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QtEndian>

#include <core/resource/network_resource.h>

#include "business/events/abstract_business_event.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include <media_server/serverutil.h>
#include "business/business_action_factory.h"
#include "core/resource_management/resource_pool.h"
#include "recorder/storage_manager.h"
#include <recording/time_period.h>
#include <media_server/settings.h>
#include "core/resource/network_resource.h"


static const qint64 EVENTS_CLEANUP_INTERVAL = 1000000ll * 3600;
static const qint64 DEFAULT_EVENT_KEEP_PERIOD = 1000000ll * 3600 * 24 * 30; // 30 days
QnEventsDB* QnEventsDB::m_instance = 0;

QnEventsDB::QnEventsDB():
    m_lastCleanuptime(0),
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
    QSqlQuery versionQuery;
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

    qint64 timestampUsec = action->getRuntimeParams().getEventTimestamp();
    QnUuid eventResId = action->getRuntimeParams().getEventResourceId();
    
    QnBusinessEventParameters actionRuntime = action->getRuntimeParams();
    if (actionRes)
        actionRuntime.setActionResourceId(actionRes->getId());
    int eventType = (int) actionRuntime.getEventType();

    insQuery.bindValue(":timestamp", timestampUsec/1000000);
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", action->getParams().pack());
    insQuery.bindValue(":runtime_params", actionRuntime.pack());
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

    QSqlQuery query(request);
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
        QnBusinessActionParameters actionParams = QnBusinessActionParameters::unpack(query.value(actionParamIdx).toByteArray());
        QnBusinessEventParameters runtimeParams = QnBusinessEventParameters::unpack(query.value(runtimeParamIdx).toByteArray());
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

    QSqlQuery actionsQuery(request);
    if (!actionsQuery.exec())
        return;

    QSqlRecord rec = actionsQuery.record();
    int actionTypeIdx = rec.indexOf(lit("action_type"));
    int actionParamIdx = rec.indexOf(lit("action_params"));
    int runtimeParamIdx = rec.indexOf(lit("runtime_params"));
    int businessRuleIdx = rec.indexOf(lit("business_rule_guid"));
//    int toggleStateIdx = rec.indexOf(lit("toggle_state"));
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
        //appendIntToBA(result, actionsQuery.value(toggleStateIdx).toInt());
        QByteArray runtimeParams = actionsQuery.value(runtimeParamIdx).toByteArray();
        appendIntToBA(result, runtimeParams.size());
        result.append(runtimeParams);

        QByteArray actionParams = actionsQuery.value(actionParamIdx).toByteArray();
        appendIntToBA(result, actionParams.size());
        result.append(actionParams);

        ++sizeField;

        //ba->set_actionparams(actionsQuery.value(actionParamIdx).toByteArray());
        //ba->set_runtimeparams(actionsQuery.value(runtimeParamIdx).toByteArray());
    }
    sizeField = qToBigEndian(sizeField);
    memcpy(result.data(), &sizeField, sizeof(int));

    qDebug() << Q_FUNC_INFO << "query time=" << t.elapsed() << "msec";
}


void QnEventsDB::migrate()
{
    QList<QnAbstractBusinessActionPtr> result;

    QString request(lit("SELECT * FROM runtime_actions"));

    QSqlQuery query(request);
    //query.prepare(request);
    if (!query.exec())
        return;

    QSqlRecord rec = query.record();
    int idIdx = rec.indexOf("id");
    int actionTypeIdx = rec.indexOf("action_type");
    int actionParamIdx = rec.indexOf("action_params");
    int runtimeParamIdx = rec.indexOf("runtime_params");
    int businessRuleIdx = rec.indexOf("business_rule_guid");
    int toggleStateIdx = rec.indexOf("toggle_state");
    int aggregationCntIdx = rec.indexOf("aggregation_count");

    QList<int> idList;

    while (query.next()) 
    {
        QnBusiness::ActionType actionType = (QnBusiness::ActionType) query.value(actionTypeIdx).toInt();
        
        QByteArray data = query.value(actionParamIdx).toByteArray();

        QnBusinessActionParameters actionParams = QnBusinessActionParameters::unpack(data);

        QnBusinessParams bParams = deserializeBusinessParams(query.value(runtimeParamIdx).toByteArray());
        QnBusinessEventParameters runtimeParams = QnBusinessEventParameters::fromBusinessParams(bParams);

        QnAbstractBusinessActionPtr action = QnBusinessActionFactory::createAction(actionType, runtimeParams);
        action->setParams(actionParams);
        action->setBusinessRuleId(QnUuid(query.value(businessRuleIdx).toString()));
        action->setToggleState( (QnBusiness::EventState) query.value(toggleStateIdx).toInt());
        action->setAggregationCount(query.value(aggregationCntIdx).toInt());

        result << action;
        idList << query.value(idIdx).toInt();
    }

    query.finish();

    for (int i = 0; i < result.size(); ++i)
    {
        QSqlQuery query2;
        query2.prepare("UPDATE runtime_actions set runtime_params=:runtime_params WHERE id =:id");
        query2.bindValue(":runtime_params", result[i]->getRuntimeParams().pack());
        query2.bindValue(":id", idList[i]);
        query2.exec();
    }

}


void QnEventsDB::init()
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(!m_instance, Q_FUNC_INFO, "QnEventsDB::init must be called once!");
    m_instance = new QnEventsDB();

    //migrate();
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

QnEventsDB::QnDbTransaction* QnEventsDB::getTransaction()
{
    return &m_tran;
}
