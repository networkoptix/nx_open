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
    case BusinessActionType::BA_CameraOutput:
        break;
    case BusinessActionType::BA_Bookmark:
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
    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    bool val = action->getToggleState() == ToggleState::On;
    conn->setPanicMode(val);
    QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceByGuid(serverGuid()));
    if (mediaServer)
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
