#include <QList>
#include "mserver_business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "api/app_server_connection.h"

bool QnMServerBusinessRuleProcessor::executeActionInternal(QnAbstractBusinessActionPtr action)
{
    if (QnBusinessRuleProcessor::executeActionInternal(action))
        return true;

    switch(action->actionType())
    {
    case BusinessActionType::BA_CameraOutput:
        break;
    case BusinessActionType::BA_Bookmark:
        break;
    case BusinessActionType::BA_CameraRecording:
        return executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>());
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
    if (action->getToggleState() == ToggleState::On)
        conn->setPanicMode(true);
    else
        conn->setPanicMode(false);
    return true;
}

bool QnMServerBusinessRuleProcessor::executeRecordingAction(QnRecordingBusinessActionPtr action)
{
    Q_ASSERT(action);
    QnSecurityCamResourcePtr camera = action->getResource().dynamicCast<QnSecurityCamResource>();
    //Q_ASSERT(camera);
    if (!camera)
        return false;

    // todo: if camera is offline function return false. Need some tries on timer event
    if (action->getToggleState() == ToggleState::On)
        return qnRecordingManager->startForcedRecording(camera, action->getStreamQuality(), action->getFps(), action->getRecordDuration());
    else
        return qnRecordingManager->stopForcedRecording(camera);
}

QString QnMServerBusinessRuleProcessor::getGuid() const
{
    return serverGuid();
}
