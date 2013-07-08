
#include <QtCore/QList>

#include "business/actions/panic_business_action.h"
#include "mserver_business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "api/app_server_connection.h"
#include "core/resource_managment/resource_pool.h"

QnMServerBusinessRuleProcessor::QnMServerBusinessRuleProcessor(): QnBusinessRuleProcessor()
{
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(onRemoveResource(QnResourcePtr)), Qt::QueuedConnection);
}

QnMServerBusinessRuleProcessor::~QnMServerBusinessRuleProcessor()
{

}

void QnMServerBusinessRuleProcessor::onRemoveResource(const QnResourcePtr &resource)
{
    QnEventsDB::instance()->removeLogForRes(resource->getId());
}

bool QnMServerBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res)
{
    bool result = QnBusinessRuleProcessor::executeActionInternal(action, res);
    if (!result) {
        switch(action->actionType())
        {
        case BusinessActionType::Bookmark:
            // TODO: implement me
            break;
        case BusinessActionType::CameraOutput:
        case BusinessActionType::CameraOutputInstant:
            result = triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>(), res);
            break;
        case BusinessActionType::CameraRecording:
            result = executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>(), res);
            break;
        case BusinessActionType::PanicRecording:
            result = executePanicAction(action.dynamicCast<QnPanicBusinessAction>());
            break;
        default:
            break;
        }
    }
    
    if (result)
        QnEventsDB::instance()->saveActionToDB(action, res);

    return result;
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
    if (action->getToggleState() == Qn::OnState)
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
        if (action->getToggleState() == Qn::OnState)
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

    bool instant = action->actionType() == BusinessActionType::CameraOutputInstant;

    int autoResetTimeout = instant
            ? 30*1000
            : qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    bool on = instant
            ? true
            : action->getToggleState() == Qn::OnState;

    return securityCam->setRelayOutputState(
                relayOutputId,
                on,
                autoResetTimeout );
}

