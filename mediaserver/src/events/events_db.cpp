#include "events_db.h"
#include "business/events/abstract_business_event.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"
#include "business/business_action_factory.h"

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

    qint64 timestampUsec = QnBusinessEventRuntime::getEventTimestamp(action->getRuntimeParams());
    int eventResId = QnBusinessEventRuntime::getEventResourceId(action->getRuntimeParams());
    
    QnBusinessParams actionRuntime = action->getRuntimeParams();
    if (actionRes)
        QnBusinessActionRuntime::setActionResourceId(&actionRuntime, actionRes->getId().toInt());
    int eventType = (int) QnBusinessEventRuntime::getEventType(actionRuntime);

    insQuery.bindValue(":timestamp", QDateTime::fromMSecsSinceEpoch(timestampUsec/1000));
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", serializeBusinessParams(action->getParams()));
    insQuery.bindValue(":runtime_params", serializeBusinessParams(action->getRuntimeParams()));
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


QList<QnAbstractBusinessActionPtr> QnEventsDB::getActions(QnTimePeriod period) const
{
    QMutexLocker lock(&m_mutex);

    QList<QnAbstractBusinessActionPtr> result;

    QSqlQuery query(m_sdb);
    if (period.durationMs != -1) {
        query.prepare("SELECT * FROM runtime_actions where timestamp between :timefrom and :timeto;");
        query.bindValue(":timefrom", QDateTime::fromMSecsSinceEpoch(period.startTimeMs));
        query.bindValue(":timeto", QDateTime::fromMSecsSinceEpoch(period.endTimeMs()));
    }
    else {
        query.prepare("SELECT * FROM runtime_actions where timestamp > :timefrom;");
        query.bindValue(":timefrom", QDateTime::fromMSecsSinceEpoch(period.startTimeMs));
    }
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
        QnBusinessParams actionParams = deserializeBusinessParams(query.value(actionParamIdx).toByteArray());
        QnBusinessParams runtimeParams = deserializeBusinessParams(query.value(runtimeParamIdx).toByteArray());
        QnAbstractBusinessActionPtr action = QnBusinessActionFactory::createAction(actionType, runtimeParams);
        action->setParams(actionParams);
        action->setBusinessRuleId(query.value(businessRuleIdx).toInt());
        action->setToggleState( (ToggleState::Value) query.value(toggleStateIdx).toInt());
        action->setAggregationCount(query.value(aggregationCntIdx).toInt());

        result << action;
    }

    return result;
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
