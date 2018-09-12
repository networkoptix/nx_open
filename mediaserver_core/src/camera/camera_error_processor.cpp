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
#include <nx/network/http/http_types.h>

namespace nx::mediaserver::camera {

ErrorProcessor::ErrorProcessor(QnMediaServerModule* serverModule)
{}

/**
 * General sink for various stream errors signals. Dispatched further according to the error CODE.
 */
void ErrorProcessor::onStreamReaderError(
    QnAbstractMediaStreamDataProvider* streamReader,
    QnAbstractMediaStreamDataProvider::ErrorCode code)
{
    switch (code)
    {
        case QnAbstractMediaStreamDataProvider::ErrorCode::streamIssue:
            processStreamIssueError(streamReader);
            break;
        case QnAbstractMediaStreamDataProvider::ErrorCode::noError:
            processNoError(streamReader);
            break;
        default:
            break;
    }
}

void ErrorProcessor::processNoError(QnAbstractMediaStreamDataProvider* streamReader)
{
    auto camera = streamReader->getResource().dynamicCast<resource::Camera>();
    NX_ASSERT(camera);
    if (!camera)
        return;
    camera->setLastMediaIssue(CameraDiagnostics::NoErrorResult());
    if (camera->getStatus() == Qn::Unauthorized || camera->getStatus() == Qn::Offline)
        camera->setStatus(Qn::Online);
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
void ErrorProcessor::processStreamIssueError(QnAbstractMediaStreamDataProvider* streamReader)
{
    auto ownerResource = streamReader->getResource().dynamicCast<resource::Camera>();
    NX_ASSERT(ownerResource);
    if (!ownerResource || !ownerResource->isInitialized())
        return;

    auto videoCamera = streamReader->getOwner().dynamicCast<QnVideoCamera>();
    NX_ASSERT(videoCamera);
    if (!videoCamera)
        return;

    if (!ownerResource->getStatus() == Qn::Unauthorized)
        return; //< Avoid offline->unauthorized->offline loop.

    auto mediaStreamProvider = dynamic_cast<QnAbstractMediaStreamProvider*>(streamReader);

    if (mediaStreamProvider
        && mediaStreamProvider->openStreamResult().errorCode == CameraDiagnostics::ErrorCode::notAuthorised)
    {
        ownerResource->setStatus(Qn::Unauthorized);
        return;
    }

    if (!streamReader->reinitResourceOnStreamError())
        return;

    const auto kMaxTimeFromPreviousFrameUs = 5 * 1000 * 1000;
    auto hasGopData = [&](Qn::StreamIndex streamIndex)
        {
            const auto nowUs = qnSyncTime->currentUSecsSinceEpoch();
            for (int i = 0; i < ownerResource->getVideoLayout()->channelCount(); ++i)
            {
                auto lastFrame = videoCamera->getLastVideoFrame(streamIndex == Qn::StreamIndex::primary, i);
                if (lastFrame && nowUs - lastFrame->timestamp < kMaxTimeFromPreviousFrameUs)
                    return true;
            }
            return false;
        };
    bool gotVideoFrameRecently =
        hasGopData(Qn::StreamIndex::primary) || hasGopData(Qn::StreamIndex::secondary);

    if (!gotVideoFrameRecently)
    {
        if (streamReader->getStatistics(0)->getTotalData() > 0)
            ownerResource->setLastMediaIssue(CameraDiagnostics::BadMediaStreamResult());
        else
            ownerResource->setLastMediaIssue(CameraDiagnostics::NoMediaStreamResult());

        ownerResource->setStatus(Qn::Offline);
    }
}

} // namespace nx::mediaserver::camera