#include "video_camera_mock.h"

#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

MediaServerVideoCameraMock::MediaServerVideoCameraMock():
    m_usageCounter(0)
{
}

QSharedPointer<QnLiveStreamProvider> MediaServerVideoCameraMock::getPrimaryReader()
{
    return QSharedPointer<QnLiveStreamProvider>();
}

QSharedPointer<QnLiveStreamProvider> MediaServerVideoCameraMock::getSecondaryReader()
{
    return QSharedPointer<QnLiveStreamProvider>();
}

QnLiveStreamProviderPtr MediaServerVideoCameraMock::getLiveReader(
    QnServer::ChunksCatalog /*catalog*/,
    bool /*ensureInitialized*/)
{
    return QnLiveStreamProviderPtr();
}

int MediaServerVideoCameraMock::copyLastGop(
    bool /*primaryLiveStream*/,
    qint64 /*skipTime*/,
    QnDataPacketQueue& /*dstQueue*/,
    int /*cseq*/,
    bool /*iFramesOnly*/)
{
    return 0;
}

QnConstCompressedVideoDataPtr MediaServerVideoCameraMock::getLastVideoFrame(
    bool /*primaryLiveStream*/,
    int /*channel*/) const
{
    return QnConstCompressedVideoDataPtr();
}

QnConstCompressedAudioDataPtr MediaServerVideoCameraMock::getLastAudioFrame(
    bool /*primaryLiveStream*/) const
{
    return QnConstCompressedAudioDataPtr();
}

std::unique_ptr<QnConstDataPacketQueue> MediaServerVideoCameraMock::getFrameSequenceByTime(
    bool /*primaryLiveStream*/,
    qint64 /*time*/,
    int /*channel*/,
    nx::api::ImageRequest::RoundMethod /*roundMethod*/) const
{
    return nullptr;
}

void MediaServerVideoCameraMock::beforeStop()
{
}

bool MediaServerVideoCameraMock::isSomeActivity() const
{
    return false;
}

void MediaServerVideoCameraMock::stopIfNoActivity()
{
}

void MediaServerVideoCameraMock::updateActivity()
{
}

void MediaServerVideoCameraMock::inUse(void* /*user*/)
{
    ++m_usageCounter;
}

void MediaServerVideoCameraMock::notInUse(void* /*user*/)
{
    ASSERT_GT(m_usageCounter, 0);
    --m_usageCounter;
}

const MediaStreamCache* MediaServerVideoCameraMock::liveCache(MediaQuality /*streamQuality*/) const
{
    NX_GTEST_ASSERT_GT(m_usageCounter, 0);
    return nullptr;
}

MediaStreamCache* MediaServerVideoCameraMock::liveCache(MediaQuality /*streamQuality*/)
{
    NX_GTEST_ASSERT_GT(m_usageCounter, 0);
    return nullptr;
}

nx::mediaserver::hls::LivePlaylistManagerPtr MediaServerVideoCameraMock::hlsLivePlaylistManager(
    MediaQuality /*streamQuality*/) const
{
    NX_GTEST_ASSERT_GT(m_usageCounter, 0);
    return nullptr;
}

bool MediaServerVideoCameraMock::ensureLiveCacheStarted(
    MediaQuality /*streamQuality*/,
    qint64 /*targetDurationUSec*/)
{
    return false;
}

QnResourcePtr MediaServerVideoCameraMock::resource() const
{
    return QnResourcePtr();
}
