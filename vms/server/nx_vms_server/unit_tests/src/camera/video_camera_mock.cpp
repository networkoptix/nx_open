#include "video_camera_mock.h"

#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

namespace nx::vms::server::test {

VideoCameraMock::VideoCameraMock():
    m_usageCounter(0)
{
}

QnLiveStreamProviderPtr VideoCameraMock::getLiveReader(
    QnServer::ChunksCatalog /*catalog*/,
    bool /*ensureInitialized*/, bool /*createIfNotExist*/)
{
    return QnLiveStreamProviderPtr();
}

int VideoCameraMock::copyLastGop(
    StreamIndex /*streamIndex*/,
    qint64 /*skipTime*/,
    QnDataPacketQueue& /*dstQueue*/,
    bool /*iFramesOnly*/)
{
    return 0;
}

QnConstCompressedVideoDataPtr VideoCameraMock::getLastVideoFrame(
    StreamIndex /*streamIndex*/,
    int /*channel*/) const
{
    return QnConstCompressedVideoDataPtr();
}

QnConstCompressedAudioDataPtr VideoCameraMock::getLastAudioFrame(
    StreamIndex /*streamIndex*/) const
{
    return QnConstCompressedAudioDataPtr();
}

std::unique_ptr<QnConstDataPacketQueue> VideoCameraMock::getFrameSequenceByTime(
    StreamIndex /*streamIndex*/,
    qint64 /*time*/,
    int /*channel*/,
    nx::api::ImageRequest::RoundMethod /*roundMethod*/) const
{
    return nullptr;
}

void VideoCameraMock::beforeStop()
{
}

bool VideoCameraMock::isSomeActivity() const
{
    return false;
}

void VideoCameraMock::stopIfNoActivity()
{
}

void VideoCameraMock::updateActivity()
{
}

void VideoCameraMock::inUse(void* /*user*/)
{
    ++m_usageCounter;
}

void VideoCameraMock::notInUse(void* /*user*/)
{
    ASSERT_GT(m_usageCounter, 0);
    --m_usageCounter;
}

const MediaStreamCache* VideoCameraMock::liveCache(MediaQuality /*streamQuality*/) const
{
    NX_GTEST_ASSERT_GT(m_usageCounter, 0);
    return nullptr;
}

MediaStreamCache* VideoCameraMock::liveCache(MediaQuality /*streamQuality*/)
{
    NX_GTEST_ASSERT_GT(m_usageCounter, 0);
    return nullptr;
}

hls::LivePlaylistManagerPtr VideoCameraMock::hlsLivePlaylistManager(
    MediaQuality /*streamQuality*/) const
{
    NX_GTEST_ASSERT_GT(m_usageCounter, 0);
    return nullptr;
}

bool VideoCameraMock::ensureLiveCacheStarted(
    MediaQuality /*streamQuality*/,
    qint64 /*targetDurationUSec*/)
{
    return false;
}

QnResourcePtr VideoCameraMock::resource() const
{
    return QnResourcePtr();
}

} // namespace nx::vms::server::test
