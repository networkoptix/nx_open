/**
 * Server-side physical cameras errors processing abstraction layer. Aimed to keep common errors
 * (such as UNAUTHORIZED or BAD_STREAM) processing in a single place and not to spread/duplicate it
 * over specific concrete cameras/streams classes.
 */

#include "camera_error_processor.h"

#include <nx/mediaserver/resource/camera.h>
#include <camera/video_camera.h>
#include <camera/camera_pool.h>
#include <utils/common/synctime.h>
#include <providers/spush_media_stream_provider.h>
#include <nx/network/http/http_types.h>

namespace nx::mediaserver::camera {

ErrorProcessor::ErrorProcessor(QnMediaServerModule* serverModule)
{}

/**
 * General sink for various stream errors signals. Dispatched further according to the error CODE.
 */
void ErrorProcessor::onStreamReaderError(CLServerPushStreamReader* streamReader, Code code)
{
    switch (code)
    {
        case Code::frameLost:
            processFrameLostError(streamReader);
            break;
        default:
            break;
    }
}

/**
 * Processes FRAME_LOST signal from a stream reader. If lost frames count exceeds certain amount,
 * undertakes further actions with the stream owner (camera) depending on the stream last error,
 * specifically, if stream status is:
 *   Unauthorized -> set owner's status to UNAUTHORIZED
 *   Otherwise ->
 *     No frames last few seconds from other streams -> set owner's status to OFFLINE
 *     Otherwise -> keep working
 */
void ErrorProcessor::processFrameLostError(CLServerPushStreamReader* streamReader)
{
    streamReader->setNeedKeyData();
    auto videoCamera = streamReader->getOwner().dynamicCast<QnVideoCamera>();
    NX_ASSERT(videoCamera);
    if (!videoCamera)
        return;

    auto ownerResource = videoCamera->resource().dynamicCast<resource::Camera>();
    NX_ASSERT(ownerResource);
    if (!ownerResource || !ownerResource->isInitialized())
        return;

    streamReader->setLostFramesCount(streamReader->lostFramesCount() + 1);
    if (streamReader->lostFramesCount() < MAX_LOST_FRAME)
        return;

    if (!streamReader->canChangeStatus() || ownerResource->getStatus() == Qn::Unauthorized)
        return; //< Avoid offline->unauthorized->offline loop.

    if (streamReader->getLastResponseCode() == network::http::StatusCode::unauthorized)
    {
        ownerResource->setStatus(Qn::Unauthorized);
        streamReader->setLostFramesCount(0);
    }
    else
    {
        const auto kMaxTimeFromPreviousFrameUs = 5 * 1000 * 1000;
        auto nowUs = qnSyncTime->currentMSecsSinceEpoch() * 1000LL;
        bool hasUpToDateFrames = false;

        for (int i = 0; i < ownerResource->getVideoLayout()->channelCount(); ++i)
        {
            auto lastPrimaryFrame = videoCamera->getLastVideoFrame(true, i);
            auto lastSecondaryFrame = videoCamera->getLastVideoFrame(false, i);
            if ((lastPrimaryFrame && lastPrimaryFrame->timestamp - nowUs < kMaxTimeFromPreviousFrameUs)
                || (lastSecondaryFrame && lastSecondaryFrame->timestamp - nowUs < kMaxTimeFromPreviousFrameUs))
            {
                hasUpToDateFrames = true;
                break;
            }
        }

        if (!hasUpToDateFrames)
        {
            if (streamReader->totalFramesCount() > 0)
                ownerResource->setLastMediaIssue(CameraDiagnostics::BadMediaStreamResult());
            else
                ownerResource->setLastMediaIssue(CameraDiagnostics::NoMediaStreamResult());

            ownerResource->setStatus(Qn::Offline);
            streamReader->setLostFramesCount(0);
            streamReader->reportConnectionLost();
        }
    }
}

} // namespace nx::mediaserver::camera