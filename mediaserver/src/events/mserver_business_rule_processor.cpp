#include <QList>
#include "mserver_business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "api/app_server_connection.h"
#include "core/resource_managment/resource_pool.h"
#include "utils/common/synctime.h"

static const qint64 EVENTS_CLEANUP_INTERVAL = 1000000ll * 3600;
static const qint64 DEFAULT_EVENT_KEEP_PERIOD = 1000000ll * 3600 * 24 * 30; // 30 days

QnMServerBusinessRuleProcessor::QnMServerBusinessRuleProcessor():
    QnBusinessRuleProcessor(),
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

QnMServerBusinessRuleProcessor::~QnMServerBusinessRuleProcessor()
{
}

void QnMServerBusinessRuleProcessor::setEventLogPeriod(qint64 periodUsec)
{
    m_eventKeepPeriod = periodUsec;
}

bool QnMServerBusinessRuleProcessor::cleanupEvents()
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

bool QnMServerBusinessRuleProcessor::saveActionToDB(QnAbstractBusinessActionPtr action)
{
    if (!m_sdb.isOpen())
        return false;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO runtime_actions (timestamp, action_type, action_params, runtime_params, business_rule_id, toggle_state, aggregation_count) "
        "VALUES (:timestamp, :action_type, :action_params, :runtime_params, :business_rule_id, :toggle_state, :aggregation_count);");

    qint64 timestampUsec = QnBusinessEventRuntime::getEventTimestamp(action->getRuntimeParams());

    insQuery.bindValue(":timestamp", QDateTime::fromMSecsSinceEpoch(timestampUsec/1000));
    insQuery.bindValue(":action_type", (int) action->actionType());
    insQuery.bindValue(":action_params", serializeBusinessParams(action->getParams()));
    insQuery.bindValue(":runtime_params", serializeBusinessParams(action->getRuntimeParams()));
    insQuery.bindValue(":business_rule_id", action->getBusinessRuleId().toInt());
    insQuery.bindValue(":toggle_state", (int) action->getToggleState());
    insQuery.bindValue(":aggregation_count", action->getAggregationCount());

    bool rez = insQuery.exec();
    if (rez)
        cleanupEvents();
    return rez;
}

bool QnMServerBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res)
{
    bool rez = QnBusinessRuleProcessor::executeActionInternal(action, res);
    if (!rez) {
        switch(action->actionType())
        {
        case BusinessActionType::Bookmark:
            // TODO: implement me
            break;
        case BusinessActionType::CameraOutput:
            rez = triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>(), res);
            break;
        case BusinessActionType::CameraRecording:
            rez = executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>(), res);
        case BusinessActionType::PanicRecording:
            rez = executePanicAction(action.dynamicCast<QnPanicBusinessAction>());
        default:
            break;
        }
    }

    if (rez)
        saveActionToDB(action);

    return rez;
}

bool QnMServerBusinessRuleProcessor::executePanicAction(QnPanicBusinessActionPtr action)
{
    QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceByGuid(serverGuid()));
    if (!mediaServer)
        return false;
    if (mediaServer->getPanicMode() == QnMediaServerResource::PM_User)
        return true; // ignore panic business action if panic mode turn on by user

    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    QnMediaServerResource::PanicMode val = QnMediaServerResource::PM_None;
    if (action->getToggleState() == ToggleState::On)
        val =  QnMediaServerResource::PM_BusinessEvents;
    conn->setPanicMode(val);
    mediaServer->setPanicMode(val);
    return true;
}

bool QnMServerBusinessRuleProcessor::executeRecordingAction(QnRecordingBusinessActionPtr action, QnResourcePtr res)
{
    Q_ASSERT(action);
    QnSecurityCamResourcePtr camera = res.dynamicCast<QnSecurityCamResource>();
    //Q_ASSERT(camera);
    bool rez = false;
    if (camera) {
        // todo: if camera is offline function return false. Need some tries on timer event
        if (action->getToggleState() == ToggleState::On)
            rez = qnRecordingManager->startForcedRecording(camera, action->getStreamQuality(), action->getFps(), 
                                                            action->getRecordBefore(), action->getRecordAfter(), 
                                                            action->getRecordDuration());
        else
            rez = qnRecordingManager->stopForcedRecording(camera);
    }
    return rez;
}

QString QnMServerBusinessRuleProcessor::getGuid() const
{
    return serverGuid();
}

bool QnMServerBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, QnResourcePtr resource )
{
    if( !resource )
    {
        cl_log.log( QString::fromLatin1("Received BA_CameraOutput with no resource reference. Ignoring..."), cl_logWARNING );
        return false;
    }
    QnSecurityCamResourcePtr securityCam = resource.dynamicCast<QnSecurityCamResource>();
    if( !securityCam )
    {
        cl_log.log( QString::fromLatin1("Received BA_CameraOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
            arg(resource->getId()), cl_logWARNING );
        return false;
    }
    QString relayOutputId = action->getRelayOutputId();
    //if( relayOutputId.isEmpty() )
    //{
    //    cl_log.log( QString::fromLatin1("Received BA_CameraOutput action without required parameter relayOutputID. Ignoring..."), cl_logWARNING );
    //    return false;
    //}

    int autoResetTimeout = qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    return securityCam->setRelayOutputState(
        relayOutputId,
        action->getToggleState() == ToggleState::On,
        autoResetTimeout );
}

