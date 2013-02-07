#include <QList>
#include "mserver_business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "api/app_server_connection.h"
#include "core/resource_managment/resource_pool.h"

bool QnMServerBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res)
{
    if (QnBusinessRuleProcessor::executeActionInternal(action, res))
        return true;

    switch(action->actionType())
    {
    case BusinessActionType::BA_Bookmark:
        // TODO: implement me
        break;
    case BusinessActionType::BA_CameraOutput:
        return triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>(), res);
        break;
    case BusinessActionType::BA_CameraRecording:
        return executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>(), res);
    case BusinessActionType::BA_PanicRecording:
        return executePanicAction(action.dynamicCast<QnPanicBusinessAction>());
    default:
        break;
    }
    return false;
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

