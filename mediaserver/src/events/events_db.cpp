#include "events_db.h"
#include "business/events/abstract_business_event.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"
#include "business/business_action_factory.h"
#include "api/serializer/pb_serializer.h"

static const qint64 EVENTS_CLEANUP_INTERVAL = 1000000ll * 3600;
static const qint64 DEFAULT_EVENT_KEEP_PERIOD = 1000000ll * 3600 * 24 * 30; // 30 days
QnEventsDB* QnEventsDB::m_instance = 0;

QnEventsDB::QnEventsDB():
    m_lastCleanuptime(0),
    m_eventKeepPeriod(DEFAULT_EVENT_KEEP_PERIOD)
{
    m_sdb = QSqlDatabase::addDatabase("QSQLITE");
    m_sdb.setDatabaseName(closeDirPath(getDataDirectory()) + QString(lit("mserver.sqlite")));

    if (m_sdb.open())
    {
        //
    }
    else {
        qWarning() << "can't initialize mySQL database! Actions log is not created!";
    }
}

void QnEventsDB::setEventLogPeriod(qint64 periodUsec)
{
    m_eventKeepPeriod = periodUsec;
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
        delQuery.bindValue(":timestamp", QDateTime::fromMSecsSinceEpoch((currentTime - m_eventKeepPeriod)/1000));
        rez = delQuery.exec();
    }
    return rez;
}

bool QnEventsDB::saveActionToDB(QnAbstractBusinessActionPtr action, QnResourcePtr actionRes)
{
    QMutexLocker lock(&m_mutex);

    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO runtime_actions (timestamp, action_type, action_params, runtime_params, business_rule_id, toggle_state, aggregation_count, event_type, event_resource_id, action_resource_id) "
        "VALUES (:timestamp, :action_type, :action_params, :runtime_params, :business_rule_id, :toggle_state, :aggregation_count, :event_type, :event_resource_id, :action_resource_id);");

    qint64 timestampUsec = action->getRuntimeParams().getEventTimestamp();
    int eventResId = action->getRuntimeParams().getEventResourceId();
    
    QnBusinessEventParameters actionRuntime = action->getRuntimeParams();
    if (actionRes)
        actionRuntime.setActionResourceId(actionRes->getId().toInt());
    int eventType = (int) actionRuntime.getEventType();

    insQuery.bindValue(":timestamp", QDateTime::fromMSecsSinceEpoch(timestampUsec/1000));
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", action->getParams().serialize());
    insQuery.bindValue(":runtime_params", action->getRuntimeParams().serialize());
    insQuery.bindValue(":business_rule_id", action->getBusinessRuleId().toInt());
    insQuery.bindValue(":toggle_state", (int) action->getToggleState());
    insQuery.bindValue(":aggregation_count", action->getAggregationCount());

    insQuery.bindValue(":event_type", eventType);
    insQuery.bindValue(":event_resource_id", eventResId);
    insQuery.bindValue(":action_resource_id", actionRes ? QVariant(actionRes->getId().toInt()) : QVariant());

    bool rez = insQuery.exec();
    if (rez)
        cleanupEvents();
    return rez;
}

QString QnEventsDB::toSQLDate(qint64 timeMs) const
{
    return QDateTime::fromMSecsSinceEpoch(timeMs).toString("yyyy-MM-dd hh:mm:ss");
}

QList<QnAbstractBusinessActionPtr> QnEventsDB::getActions(
    const QnTimePeriod& period,
    const QnId& cameraId, 
    const BusinessEventType::Value& eventType, 
    const BusinessActionType::Value& actionType,
    const QnId& businessRuleId) const

{
    QTime t;
    t.restart();

    QMutexLocker lock(&m_mutex);

    QList<QnAbstractBusinessActionPtr> result;

    QString request(lit("SELECT * FROM runtime_actions where"));
    if (period.durationMs != -1) {
        request += QString(lit(" timestamp between '%1' and '%2'")).arg(toSQLDate(period.startTimeMs)).arg(toSQLDate(period.endTimeMs()));
    }
    else {
        request += QString(lit(" timestamp >= '%1'")).arg(period.startTimeMs);
    }
    if (cameraId.isValid())
        request += QString(lit(" and event_resource_id = %1 ")).arg(cameraId.toInt());
    if (eventType != BusinessEventType::NotDefined)
        request += QString(lit( " and event_type = %1 ")).arg((int) eventType);
    if (actionType != BusinessActionType::NotDefined)
        request += QString(lit( " and action_type = %1 ")).arg((int) actionType);
    if (businessRuleId.isValid())
        request += QString(lit( " and  business_rule_id = %1 ")).arg(businessRuleId.toInt());

    QSqlQuery query(request);
    if (!query.exec())
        return result;

     QSqlRecord rec = query.record();
     int actionTypeIdx = rec.indexOf("action_type");
     int actionParamIdx = rec.indexOf("action_params");
     int runtimeParamIdx = rec.indexOf("runtime_params");
     int businessRuleIdx = rec.indexOf("business_rule_id");
     int toggleStateIdx = rec.indexOf("toggle_state");
     int aggregationCntIdx = rec.indexOf("aggregation_count");

    while (query.next()) 
    {
        BusinessActionType::Value actionType = (BusinessActionType::Value) query.value(actionTypeIdx).toInt();
        QnBusinessActionParameters actionParams = QnBusinessActionParameters::deserialize(query.value(actionParamIdx).toByteArray());
        QnBusinessEventParameters runtimeParams = QnBusinessEventParameters::deserialize(query.value(runtimeParamIdx).toByteArray());
        QnAbstractBusinessActionPtr action = QnBusinessActionFactory::createAction(actionType, runtimeParams);
        action->setParams(actionParams);
        action->setBusinessRuleId(query.value(businessRuleIdx).toInt());
        action->setToggleState( (ToggleState::Value) query.value(toggleStateIdx).toInt());
        action->setAggregationCount(query.value(aggregationCntIdx).toInt());

        result << action;
    }

    qDebug() << Q_FUNC_INFO << "query time=" << t.elapsed() << "msec";

    return result;
}

void QnEventsDB::getAndSerializeActions(
    QByteArray& result,
    const QnTimePeriod& period,
    const QnId& cameraId, 
    const BusinessEventType::Value& eventType, 
    const BusinessActionType::Value& actionType,
    const QnId& businessRuleId) const

{
    QTime t;
    t.restart();

    QMutexLocker lock(&m_mutex);

    QString request(lit("SELECT * FROM runtime_actions where"));
    if (period.durationMs != -1) {
        request += QString(lit(" timestamp between '%1' and '%2'")).arg(toSQLDate(period.startTimeMs)).arg(toSQLDate(period.endTimeMs()));
    }
    else {
        request += QString(lit(" timestamp >= '%1'")).arg(period.startTimeMs);
    }
    if (cameraId.isValid())
        request += QString(lit(" and event_resource_id = %1 ")).arg(cameraId.toInt());
    if (eventType != BusinessEventType::NotDefined)
        request += QString(lit( " and event_type = %1 ")).arg((int) eventType);
    if (actionType != BusinessActionType::NotDefined)
        request += QString(lit( " and action_type = %1 ")).arg((int) actionType);
    if (businessRuleId.isValid())
        request += QString(lit( " and  business_rule_id = %1 ")).arg(businessRuleId.toInt());

    QSqlQuery query(request);
    if (!query.exec())
        return;

    QnApiPbSerializer serializer;
    serializer.serializeBusinessActionList(query, result);

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
    int businessRuleIdx = rec.indexOf("business_rule_id");
    int toggleStateIdx = rec.indexOf("toggle_state");
    int aggregationCntIdx = rec.indexOf("aggregation_count");

    QList<int> idList;

    while (query.next()) 
    {
        BusinessActionType::Value actionType = (BusinessActionType::Value) query.value(actionTypeIdx).toInt();
        
        QByteArray data = query.value(actionParamIdx).toByteArray();

        QnBusinessActionParameters actionParams = QnBusinessActionParameters::deserialize(data);

        QnBusinessParams bParams = deserializeBusinessParams(query.value(runtimeParamIdx).toByteArray());
        QnBusinessEventParameters runtimeParams = QnBusinessEventParameters::fromBusinessParams(bParams);

        QnAbstractBusinessActionPtr action = QnBusinessActionFactory::createAction(actionType, runtimeParams);
        action->setParams(actionParams);
        action->setBusinessRuleId(query.value(businessRuleIdx).toInt());
        action->setToggleState( (ToggleState::Value) query.value(toggleStateIdx).toInt());
        action->setAggregationCount(query.value(aggregationCntIdx).toInt());

        result << action;
        idList << query.value(idIdx).toInt();
    }

    query.finish();

    for (int i = 0; i < result.size(); ++i)
    {
        QSqlQuery query2;
        query2.prepare("UPDATE runtime_actions set runtime_params=:runtime_params WHERE id =:id");
        query2.bindValue(":runtime_params", result[i]->getRuntimeParams().serialize());
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
