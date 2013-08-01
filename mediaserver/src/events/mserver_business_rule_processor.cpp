#include <QList>
#include "business/actions/panic_business_action.h"
#include "mserver_business_rule_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "api/app_server_connection.h"
#include "core/resource_managment/resource_pool.h"

#include "core/resource/camera_resource.h"
#include "camera/camera_pool.h"
#include "decoders/video/ffmpeg.h"

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

QImage QnMServerBusinessRuleProcessor::getEventScreenshot(const QnBusinessEventParameters& params, QSize dstSize) const {
    // TODO: rvasilenko, please review

    QImage result;

    // By now only motion screenshot is supported
    if (params.getEventType() != BusinessEventType::Camera_Motion)
        return result;

    QnVirtualCameraResourcePtr res = qSharedPointerDynamicCast<QnVirtualCameraResource> (QnResourcePool::instance()->getResourceById(params.getEventResourceId()));
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);

    if (!camera)
        return result;

    QnCompressedVideoDataPtr video = camera->getLastVideoFrame(true);
    if (!video)
        video = camera->getLastVideoFrame(false);

    if (!video)
        return result;

    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);

    QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );
    bool gotFrame = (res->getStatus() == QnResource::Online || res->getStatus() == QnResource::Recording) && decoder.decode(video, &outFrame);
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
