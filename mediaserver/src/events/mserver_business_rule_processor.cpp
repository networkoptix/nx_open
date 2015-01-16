#include "mserver_business_rule_processor.h"

#include <QtCore/QList>

#include "api/app_server_connection.h"

#include "business/actions/panic_business_action.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>

#include "camera/camera_pool.h"
#include "decoders/video/ffmpeg.h"

#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>

#include <media_server/serverutil.h>

#include <utils/math/math.h>
#include <utils/common/log.h>
#include "camera/get_image_helper.h"
#include "core/resource_management/resource_properties.h"

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

bool QnMServerBusinessRuleProcessor::executeActionInternal(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    bool result = QnBusinessRuleProcessor::executeActionInternal(action, res);
    if (!result) {
        switch(action->actionType())
        {
        case QnBusiness::BookmarkAction:
            result = executeBookmarkAction(action, res);
            break;
        case QnBusiness::CameraOutputAction:
        case QnBusiness::CameraOutputOnceAction:
            result = triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>(), res);
            break;
        case QnBusiness::CameraRecordingAction:
            result = executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>(), res);
            break;
        case QnBusiness::PanicRecordingAction:
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

bool QnMServerBusinessRuleProcessor::executePanicAction(const QnPanicBusinessActionPtr& action)
{
    const QnResourcePtr& mediaServerRes = qnResPool->getResourceById(serverGuid());
    QnMediaServerResource* mediaServer = dynamic_cast<QnMediaServerResource*> (mediaServerRes.data());
    if (!mediaServer)
        return false;
    if (mediaServer->getPanicMode() == Qn::PM_User)
        return true; // ignore panic business action if panic mode turn on by user
    
    Qn::PanicMode val = Qn::PM_None;
    if (action->getToggleState() == QnBusiness::ActiveState)
        val =  Qn::PM_BusinessEvents;
    mediaServer->setPanicMode(val);
    propertyDictionary->saveParams(mediaServer->getId());
    return true;
}

bool QnMServerBusinessRuleProcessor::executeRecordingAction(const QnRecordingBusinessActionPtr& action, const QnResourcePtr& res)
{
    Q_ASSERT(action);
    QnSecurityCamResourcePtr camera = res.dynamicCast<QnSecurityCamResource>();
    //Q_ASSERT(camera);
    bool rez = false;
    if (camera) {
        // todo: if camera is offline function return false. Need some tries on timer event
        if (action->getToggleState() == QnBusiness::ActiveState)
            rez = qnRecordingManager->startForcedRecording(
                camera,
                action->getStreamQuality(), action->getFps(), 
                0, /* Record-before setup is forbidden */
                action->getRecordAfter(), 
                action->getRecordDuration());
        else
            rez = qnRecordingManager->stopForcedRecording(camera);
    }
    return rez;
}

bool QnMServerBusinessRuleProcessor::executeBookmarkAction(const QnAbstractBusinessActionPtr &action, const QnResourcePtr &resource) {
    Q_ASSERT(action);
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return false;

    QnUuid ruleId = action->getBusinessRuleId();

    if (action->getToggleState() == QnBusiness::ActiveState) {
        m_runningBookmarkActions[ruleId] = QDateTime::currentMSecsSinceEpoch();
        return true;
    }

    if (!m_runningBookmarkActions.contains(ruleId))
        return false;

    qint64 startTime = m_runningBookmarkActions.take(ruleId);
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.startTimeMs = startTime;
    bookmark.durationMs = endTime - startTime;

    bookmark.name = "Auto-Generated Bookmark";
    bookmark.description = QString("Rule %1\nFrom %2 to %3")
        .arg(ruleId.toString())
        .arg(QDateTime::fromMSecsSinceEpoch(startTime).toString())
        .arg(QDateTime::fromMSecsSinceEpoch(endTime).toString());

    return qnStorageMan->addBookmark(camera->getUniqueId().toUtf8(), bookmark, true);
}


QnUuid QnMServerBusinessRuleProcessor::getGuid() const {
    return serverGuid();
}

bool QnMServerBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, const QnResourcePtr& resource )
{
    if( !resource )
    {
        NX_LOG( lit("Received BA_CameraOutput with no resource reference. Ignoring..."), cl_logWARNING );
        return false;
    }
    QnSecurityCamResource* securityCam = dynamic_cast<QnSecurityCamResource*>(resource.data());
    if( !securityCam )
    {
        NX_LOG( lit("Received BA_CameraOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
            arg(resource->getId().toString()), cl_logWARNING );
        return false;
    }
    QString relayOutputId = action->getRelayOutputId();
    //if( relayOutputId.isEmpty() )
    //{
    //    NX_LOG( lit("Received BA_CameraOutput action without required parameter relayOutputID. Ignoring..."), cl_logWARNING );
    //    return false;
    //}

    bool instant = action->actionType() == QnBusiness::CameraOutputOnceAction;

    int autoResetTimeout = instant
            ? 30*1000
            : qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    bool on = instant
            ? true
            : action->getToggleState() == QnBusiness::ActiveState;

    return securityCam->setRelayOutputState(
                relayOutputId,
                on,
                autoResetTimeout );
}

QByteArray QnMServerBusinessRuleProcessor::getEventScreenshotEncoded(const QnBusinessEventParameters& params, QSize dstSize) const 
{
    // By now only motion screenshot is supported
    if (params.eventType != QnBusiness::CameraMotionEvent)
        return QByteArray();

    const QnResourcePtr& cameraRes = QnResourcePool::instance()->getResourceById(params.eventResourceId);
    QSharedPointer<CLVideoDecoderOutput> frame = QnGetImageHelper::getImage(cameraRes.dynamicCast<QnVirtualCameraResource>(), DATETIME_NOW, dstSize);
    return frame ? QnGetImageHelper::encodeImage(frame, "jpg") : QByteArray();
}
