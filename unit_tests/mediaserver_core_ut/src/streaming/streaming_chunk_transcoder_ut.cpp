#include <gtest/gtest.h>

#include <nx/core/access/access_types.h>

#include <camera/camera_pool.h>
#include <common/common_module.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/settings.h>
#include <streaming/streaming_chunk_transcoder.h>
#include <streaming/streaming_params.h>
#include <test_support/resource/camera_resource_stub.h>

#include "../camera/video_camera_mock.h"

namespace nx {
namespace vms {
namespace mediaserver {
namespace test {

class StreamingChunkTranscoder:
    public ::testing::Test
{
public:
    StreamingChunkTranscoder():
        m_commonModule(false, nx::core::access::Mode::cached),
        m_resourcePool(&m_commonModule),
        m_videoCameraPool(m_settings, &m_resourcePool),
        m_streamingChunkTranscoder(
            &m_resourcePool,
            &m_videoCameraPool,
            static_cast<::StreamingChunkTranscoder::Flags>(0))
    {
    }

protected:
    void givenMediaSourceWithPartialQualityAvailability()
    {
        m_cameraResourceId = QnUuid::createUuid();

        auto cameraResource = QnResourcePtr(new CameraResourceStub());
        cameraResource->setId(m_cameraResourceId);
        m_resourcePool.addResource(cameraResource);
        ASSERT_TRUE(m_videoCameraPool.addVideoCamera(
            cameraResource, QnVideoCameraPtr(new MediaServerVideoCameraMock())));

        m_streamingChunkKey = StreamingChunkCacheKey(
            m_cameraResourceId.toSimpleString(),
            0,
            "mpeg2ts",
            QString(),
            0,
            std::chrono::seconds(1),
            MediaQuality::MEDIA_Quality_Low,
            {{StreamingParams::LIVE_PARAM_NAME, QString()}});

        m_streamingChunk = std::make_shared<StreamingChunk>(
            m_streamingChunkKey,
            1024);
    }

    void whenStartTranscodingOfUnavailableQuality()
    {
        m_prevStartTranscodeResult = m_streamingChunkTranscoder.transcodeAsync(
            m_streamingChunkKey,
            m_streamingChunk);
    }

    void thenStartTranscodingFailed()
    {
        ASSERT_FALSE(m_prevStartTranscodeResult);
    }
    
private:
    MSSettings m_settings;
    QnCommonModule m_commonModule;

    QnResourcePool m_resourcePool;
    QnVideoCameraPool m_videoCameraPool;
    ::StreamingChunkTranscoder m_streamingChunkTranscoder;
    QnUuid m_cameraResourceId;
    StreamingChunkCacheKey m_streamingChunkKey;
    std::shared_ptr<StreamingChunk> m_streamingChunk;
    bool m_prevStartTranscodeResult = false;
};

TEST_F(StreamingChunkTranscoder, transcode_fails_if_desired_quality_is_not_available)
{
    givenMediaSourceWithPartialQualityAvailability();
    whenStartTranscodingOfUnavailableQuality();
    thenStartTranscodingFailed();
}

} // namespace test
} // namespace mediaserver
} // namespace vms
} // namespace nx
