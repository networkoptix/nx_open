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
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    conn->setPanicModeSync(val);
    mediaServer->setPanicMode(val);
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

QImage QnMServerBusinessRuleProcessor::getEventScreenshot(const QnBusinessEventParameters& params, QSize dstSize) const {
    // TODO: this code is copy-pasted from getImageHandler. need refactoring to avoid code duplicate

    QImage result;

    // By now only motion screenshot is supported
    if (params.getEventType() != QnBusiness::CameraMotionEvent)
        return result;

    const QnResourcePtr& cameraRes = QnResourcePool::instance()->getResourceById(params.getEventResourceId());
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(cameraRes);
    const QnVirtualCameraResource* res = dynamic_cast<const QnVirtualCameraResource*>(cameraRes.data());

    if (!camera)
        return result;

    QnConstCompressedVideoDataPtr video = camera->getLastVideoFrame(true);
    if (!video)
        video = camera->getLastVideoFrame(false);

    if (!video)
        return result;

    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);

    QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );
    bool gotFrame = (res->getStatus() == Qn::Online || res->getStatus() == Qn::Recording) && decoder.decode(video, &outFrame);
    if (!gotFrame)
        return result;

    double ar = decoder.getSampleAspectRatio() * outFrame->width / outFrame->height;
    if (!dstSize.isEmpty()) {
        dstSize.setHeight(qPower2Ceil((unsigned) dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned) dstSize.width(), 4));
    }
    else if (dstSize.height() > 0) {
        dstSize.setHeight(qPower2Ceil((unsigned) dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned) (dstSize.height()*ar), 4));
    }
    else if (dstSize.width() > 0) {
        dstSize.setWidth(qPower2Ceil((unsigned) dstSize.width(), 4));
        dstSize.setHeight(qPower2Ceil((unsigned) (dstSize.width()/ar), 4));
    }
    else {
        dstSize = QSize(outFrame->width, outFrame->height);
    }
    dstSize.setWidth(qMin(dstSize.width(), outFrame->width));
    dstSize.setHeight(qMin(dstSize.height(), outFrame->height));

    if (dstSize.width() < 8 || dstSize.height() < 8)
        return result;

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, qPower2Ceil(static_cast<quint32>(dstSize.width()), 8), dstSize.height());
    uchar* scaleBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));
    SwsContext* scaleContext = sws_getContext(outFrame->width, outFrame->height, PixelFormat(outFrame->format), 
        dstSize.width(), dstSize.height(), PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);

    int dstLineSize[4];
    quint8* dstBuffer[4];
    dstLineSize[0] = qPower2Ceil(static_cast<quint32>(dstSize.width() * 4), 32);
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(scaleBuffer, dstSize.width(), dstSize.height(), dstLineSize[0], QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = scaleBuffer;
    sws_scale(scaleContext, outFrame->data, outFrame->linesize, 0, outFrame->height, dstBuffer, dstLineSize);

    result = image.copy();

    sws_freeContext(scaleContext);
    qFreeAligned(scaleBuffer);

    return result;
}
