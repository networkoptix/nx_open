#include <QList>
#include "mserver_business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "api/app_server_connection.h"
#include "core/resource_managment/resource_pool.h"

void QnMServerBusinessRuleProcessor::saveActionToDB(QnAbstractBusinessActionPtr action)
{
    if (!my_InsQuery)
        return;

    qint64 timestampUsec = QnBusinessEventRuntime::getEventTimestamp(action->getRuntimeParams());

    my_InsQuery->bindValue(":timestamp", QDateTime::fromMSecsSinceEpoch(timestampUsec/1000));
    my_InsQuery->bindValue(":action_type", (int) action->actionType());
    my_InsQuery->bindValue(":action_params", serializeBusinessParams(action->getParams()));
    my_InsQuery->bindValue(":runtime_params", serializeBusinessParams(action->getRuntimeParams()));
    my_InsQuery->bindValue(":business_rule_id", action->getBusinessRuleId().toInt());
    my_InsQuery->bindValue(":toggle_state", (int) action->getToggleState());
    my_InsQuery->bindValue(":aggregation_count", action->getAggregationCount());

    bool insOK = my_InsQuery->exec();
    insOK = insOK;
}

QnMServerBusinessRuleProcessor::QnMServerBusinessRuleProcessor():
    QnBusinessRuleProcessor(),
    my_InsQuery(0)
{
    m_sdb = QSqlDatabase::addDatabase("QSQLITE");
    m_sdb.setDatabaseName(closeDirPath(getDataDirectory()) + QString(lit("mserver.sqlite")));

    if (m_sdb.open())
    {
        my_InsQuery = new QSqlQuery(m_sdb);
        my_InsQuery->prepare("INSERT INTO runtime_actions (timestamp, action_type, action_params, runtime_params, business_rule_id, toggle_state, aggregation_count) "
            "VALUES (:timestamp, :action_type, :action_params, :runtime_params, :business_rule_id, :toggle_state, :aggregation_count);");
    }
    else {
        qWarning() << "can't initialize mySQL database! Actions log is not created!";
    }
}

QnMServerBusinessRuleProcessor::~QnMServerBusinessRuleProcessor()
{
    delete my_InsQuery;
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

