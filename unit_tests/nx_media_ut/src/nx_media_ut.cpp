#include <gtest/gtest.h>

#include <common/common_globals.h>

#include <nx/media/abstract_video_decoder.h>
#include <nx/media/video_decoder_registry.h>
#include <nx/media/media_player.h>

#include <nx/streaming/archive_stream_reader.h>
#include <core/resource/camera_resource.h>

#include <core/resource/media_server_resource.h>

#include <common/common_module.h>
#include <utils/media/ffmpeg_initializer.h>
#include <core/resource_management/resource_pool.h>

// Config for debugging the tests.
static const struct
{
    const bool enableHangOnFinish = false;
    const bool forceLog = false;
    const bool logMockCameraStreams = true;
} conf;
#include <nx/utils/test_support/test_utils.h>

namespace nx {
namespace media {
namespace test {

namespace {

//-------------------------------------------------------------------------------------------------
// Test/mock classes.

static const QSize kExpectedQualityLow{-1, MEDIA_Quality_Low};
static const QSize kExpectedQualityHigh{-1, MEDIA_Quality_High};

class TestPlayer: public Player
{
public:
    TestPlayer(QnArchiveStreamReader* archiveReader)
    {
        testSetOwnedArchiveReader(archiveReader);
    }

    using Player::testSetCamera;
};

class MockVideoDecoder: public AbstractVideoDecoder
{
public:
    MockVideoDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution)
    {
        QN_UNUSED(allocator, resolution);
    }

    virtual ~MockVideoDecoder() {}

    static bool isCompatible(const AVCodecID codec, const QSize& resolution)
    {
        QN_UNUSED(codec);

        if (!s_maxResolution.isEmpty()
            && (resolution.width() > s_maxResolution.width()
                || resolution.height() > s_maxResolution.height()))
        {
            LOG(lit("MockVideoDecoder::isCompatible() -> false: "
                "resolution %1 x %2 is higher than %3 x %4")
                .arg(resolution.width()).arg(resolution.height())
                .arg(s_maxResolution.width()).arg(s_maxResolution.height()));
            return false;
        }

        if (codec == s_transcodingCodec
            && !s_maxTranscodingResolution.isEmpty()
            && (resolution.width() > s_maxTranscodingResolution.width()
                || resolution.height() > s_maxTranscodingResolution.height()))
        {
            LOG(lit("MockVideoDecoder::isCompatible() -> false: "
                "For transcoding codec %1, resolution %2 x %3 is higher than %4 x %5")
                .arg(s_transcodingCodec)
                .arg(resolution.width()).arg(resolution.height())
                .arg(s_maxTranscodingResolution.width()).arg(s_maxTranscodingResolution.height()));
            return false;
        }

        return true;
    }

    static QSize maxResolution(const AVCodecID codec)
    {
        QN_UNUSED(codec);
        return s_maxResolution;
    }

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override
    {
        QN_UNUSED(compressedVideoData, outDecodedFrame);
        LOG(lit("INTERNAL ERROR: VideoDecoder::decode() called"));
        return 0;
    }

public:
    // Should be assigned externally.
    static QSize s_maxResolution;
    static AVCodecID s_transcodingCodec;
    static QSize s_maxTranscodingResolution;
};
QSize MockVideoDecoder::s_maxResolution{};
AVCodecID MockVideoDecoder::s_transcodingCodec{AV_CODEC_ID_NONE};
QSize MockVideoDecoder::s_maxTranscodingResolution{};

class MockServer: public QnMediaServerResource
{
public:
    MockServer()
    {
        setId(QnUuid::createUuid());
    }

    void setSupportsTranscoding(bool supportsTranscoding)
    {
        if (supportsTranscoding)
            setServerFlags(getServerFlags() | Qn::SF_SupportsTranscoding);
        else
            setServerFlags(getServerFlags() & ~Qn::SF_SupportsTranscoding);
    }
};

class MockCamera: public QnVirtualCameraResource
{
public:
    MockCamera(QnUuid parentId):
        m_cameraType(Qn::LC_Professional)
    {
        setId(QnUuid::createUuid());
        setParentId(parentId);

        addFlags(Qn::server_live_cam);
    }

    /** Either resolution can be empty - the corresponding stream will not be created. */
    void setStreams(QSize highResolution, QSize lowResolution)
    {
        removeProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME);

        // For mock camera, use a codec that will never match any reasonable transcoding codec.
        static const int kCodec = /*102400*/ AV_CODEC_ID_PROBE;

        const QString highStr = highResolution.isEmpty()
            ? ""
            : lit("{\"encoderIndex\":0, \"resolution\":\"%1x%2\", \"codec\":%3}")
                .arg(highResolution.width()).arg(highResolution.height()).arg(kCodec);
        const QString lowStr = lowResolution.isEmpty()
            ? ""
            : lit("{\"encoderIndex\":1, \"resolution\":\"%1x%2\", \"codec\":%3}")
                .arg(lowResolution.width()).arg(lowResolution.height()).arg(kCodec);
        const QString separator = (lowStr.isEmpty() || highStr.isEmpty()) ? lit("") : lit(", ");
        const QString json = lit("{\"streams\":[") + lowStr + separator + highStr + lit("]}");

        setProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME, json);

        const int expectedStreamsCount =
            int(!lowResolution.isEmpty()) + int(!highResolution.isEmpty());
        if ((size_t)expectedStreamsCount != mediaStreams().streams.size())
        {
            LOG(lit("INTERNAL ERROR: Failed adding camera streams: expected %1, actual %2")
                .arg(expectedStreamsCount).arg(mediaStreams().streams.size()));
            ASSERT(false);
        }

        if (conf.logMockCameraStreams)
        {
            LOG(lit("Camera streams JSON: %1").arg(json));
            debugLogStreams();
        }
    }

    virtual QString getDriverName() const override { return lit("MockCamera"); }
    virtual void setIframeDistance(int frames, int timems) override { QN_UNUSED(frames, timems); }
    virtual Qn::ResourceStatus getStatus() const override { return Qn::Online; }
    virtual Qn::LicenseType licenseType() const override { return m_cameraType; }

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override { return nullptr; }

private:
    void debugLogStreams()
    {
        for (const auto& stream: mediaStreams().streams)
        {
            if (stream.encoderIndex == CameraMediaStreamInfo::PRIMARY_STREAM_INDEX) //< High
            {
                LOG(lit("Camera High stream: %1 x %2, AVCodecID %3")
                    .arg(stream.getResolution().width()).arg(stream.getResolution().height())
                    .arg(stream.codec));
            }
            else if (stream.encoderIndex == CameraMediaStreamInfo::SECONDARY_STREAM_INDEX) //< Low
            {
                LOG(lit("Camera Low stream: %1 x %2, AVCodecID %3")
                    .arg(stream.getResolution().width()).arg(stream.getResolution().height())
                    .arg(stream.codec));
            }
            else
            {
                LOG(lit("INTERNAL ERROR: Unexpected camera stream: %1 x %2, AVCodecID %3")
                    .arg(stream.getResolution().width()).arg(stream.getResolution().height())
                    .arg(stream.codec));
                ASSERT(false);
            }
        }
    }

private:
    Qn::LicenseType m_cameraType;
};

class MockArchiveStreamReader: public QnArchiveStreamReader
{
public:
    typedef std::function<void(
        MediaQuality quality, bool fastSwitch, const QSize& resolution)> QualitySetCallback;

    MockArchiveStreamReader(
        const QnResourcePtr& camera, QualitySetCallback qualitySetCallback)
        :
        QnArchiveStreamReader(camera),
        m_qualitySetCallback(qualitySetCallback)
    {
    }

    void setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution) override
    {
        m_qualitySetCallback(quality, fastSwitch, resolution);
    }

private:
    QualitySetCallback m_qualitySetCallback;
};

//-------------------------------------------------------------------------------------------------
// Test mechanism.

class PlayerSetQualityTest
{
public:
    PlayerSetQualityTest()
    {
        QnResourcePool::instance()->clear(); //< Just in case.
        QnResourcePool::instance()->addResource(m_server);
        QnResourcePool::instance()->addResource(m_camera);
    }

    ~PlayerSetQualityTest()
    {
        QnResourcePool::instance()->clear();
    }

    // Can be called multiple times.
    void test(int sourceCodeLineNumber, const char* sourceCodeLineString,
        bool clientSupportsTranscoding,
        bool serverSupportsTranscoding,
        QSize maxTranscodingResolution,
        QSize maxDecoderResolution,
        QSize highStreamResolution,
        QSize lowStreamResolution,
        int videoQuality,
        QSize expectedQuality)
    {
        LOG(lit("=============================================================================="));
        LOG(lit("line %1: %2\n").arg(sourceCodeLineNumber).arg(sourceCodeLineString));

        VideoDecoderRegistry::instance()->setTranscodingEnabled(clientSupportsTranscoding);
        m_server->setSupportsTranscoding(serverSupportsTranscoding);
        MockVideoDecoder::s_maxResolution = maxDecoderResolution;
        MockVideoDecoder::s_transcodingCodec = m_archiveReader->getTranscodingCodec();
        MockVideoDecoder::s_maxTranscodingResolution = maxTranscodingResolution;
        m_camera->setStreams(highStreamResolution, lowStreamResolution);

        m_calledSetQuality = false;

        // Reset stored Video Quality value to guarantee that our new value will be processed.
        m_player->testSetCamera(QnResourcePtr(nullptr));
        m_player->setVideoQuality(-1);
        m_player->testSetCamera(m_camera);

        m_player->setVideoQuality(videoQuality); //< Calls handleSetQuality().

        EXPECT_TRUE(m_calledSetQuality) << "setQuality() has not been called by Player";
        if (m_calledSetQuality)
        {
            if (expectedQuality == kExpectedQualityLow)
                checkActualValues(MEDIA_Quality_Low, QSize());
            else if (expectedQuality == kExpectedQualityHigh)
                checkActualValues(MEDIA_Quality_High, QSize());
            else
                checkActualValues(MEDIA_Quality_CustomResolution, expectedQuality);
        }
    }

private:
    void checkActualValues(MediaQuality expectedMediaQuality, QSize expectedResolution)
    {
        // ASSERT_EQ not used to properly convert values to strings.

        if (expectedMediaQuality != m_actualQuality)
        {
            ADD_FAILURE() << "MediaQuality: "
                << "expected " << mediaQualityToString(expectedMediaQuality)
                << ", actual " << mediaQualityToString(m_actualQuality);
        }

        if (expectedResolution != m_actualResolution)
        {
            ADD_FAILURE() << "Resolution: "
                << "expected " << qSizeToString(expectedResolution)
                << ", actual " << qSizeToString(m_actualResolution);
        }
    }

    void handleSetQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution)
    {
        EXPECT_TRUE(fastSwitch);

        EXPECT_FALSE(m_calledSetQuality) << "setQuality() called more than once.";
        m_calledSetQuality = true;

        LOG(lit("setQuality(%1, fastSwitch: %2, %3 x %4);")
            .arg(mediaQualityToString(quality))
            .arg(fastSwitch)
            .arg(resolution.width())
            .arg(resolution.height()));

        m_actualQuality = quality;
        m_actualResolution = resolution;
    }

private:
    QnSharedResourcePointer<MockServer> m_server{new MockServer()};

    QnSharedResourcePointer<MockCamera> m_camera{new MockCamera(m_server->getId())};

    MockArchiveStreamReader* m_archiveReader{new MockArchiveStreamReader(
        m_camera, std::bind(&PlayerSetQualityTest::handleSetQuality, this, _1, _2, _3))};

    std::unique_ptr<TestPlayer> m_player{new TestPlayer(m_archiveReader)};

    bool m_calledSetQuality;

    // Test results.
    MediaQuality m_actualQuality;
    QSize m_actualResolution;
};

} // namespace

class NxMediaPlayerTest: public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        // Init singletons.
        m_common = new QnCommonModule();
        m_common->setModuleGUID(QnUuid::createUuid());
        m_common->store<QnFfmpegInitializer>(new QnFfmpegInitializer());

        VideoDecoderRegistry::instance()->reinitialize(); //< Just in case.
        VideoDecoderRegistry::instance()->addPlugin<MockVideoDecoder>();
    }

    static void TearDownTestCase()
    {
        delete m_common;
        m_common = nullptr;
        VideoDecoderRegistry::instance()->reinitialize();
    }

private:
    static QnCommonModule* m_common;
};
QnCommonModule* NxMediaPlayerTest::m_common = nullptr;

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(NxMediaPlayerTest, SetQuality)
{
    PlayerSetQualityTest test;

    static const QSize none{};
    static const QSize high{kExpectedQualityHigh};
    static const QSize low{kExpectedQualityLow};
    static const Player::VideoQuality hi{Player::HighVideoQuality};
    static const Player::VideoQuality lo{Player::LowVideoQuality};
    #define T(...) test.test(__LINE__, #__VA_ARGS__, __VA_ARGS__)

    // NOTE: kMaxTranscodingResolution is defined in Player private as 1920 x 1080.

  // TranscoSupported                                                       Quality
  //  Client  Server  MaxTranscoRes MaxDecoderRes HighStreamRes LowStreamRes  hi/lo  Expected

    // High stream requested.
    T(false,  false,  none,         {1920,1080},  {1920,1080},  { 320, 240},    hi,  high       );
    T(false,  false,  none,         {1920,1080},  {1920,1080},  { 320, 240},  1080,  high       );
    T(false,  false,  none,         {1920,1080},  {1920,1080},  { 320, 240},    hi,  high       );
    T(false,  false,  none,         { 640, 480},  {1920,1080},  { 320, 240},    hi,  low        );
    T(true,   true,   none,         { 640, 480},  {1280, 720},  { 320, 240},    hi,  low        );
    T(true,   true,   none,         {1920,1080},  {2560,1440},  { 320, 240},    hi,  {1920,1080});
    T(true,   true,   none,         {1920,1080},  none,         { 320, 240},    hi,  low        );

    // Low stream requested.
    T(true,   true,   none,         {1920,1080},  {1920,1080},  { 320, 240},    lo,  low        );
    T(true,   true,   none,         {1920,1080},  {1920,1080},  { 320, 240},   240,  low        );

    // Invalid Video Quality.
    T(true,   true,   none,         none,         {1920,1080},  { 640, 480},  -113,  low        );

    // Transcoding requested and is available.
    T(true,   true,   none,         none,         none,         { 640, 480},   360,  { 480, 360});

    // Transcoding requested but is not available.
    T(false,  false,  none,         none,         none,         { 640, 480},   360,  low        );
    T(true,   false,  none,         none,         none,         { 640, 480},   360,  low        );
    T(false,  true,   none,         none,         none,         { 640, 480},   360,  low        );

    // Transcoding to the requested resolution which is within limits.
    T(true,   true,   none,         {1920,1080},  {1920,1080},  { 320, 240},   720,  {1280, 720});

    // Transcoding to a resolution limited by MaxDecoderRes.
    T(true,   true,   none,         {1280, 720},  {2560,1440},  { 320, 240},  1080,  {1280, 720});

    // Transcoding to a resolution limited by kMaxTranscodingResolution.
    T(true,   true,   none,         none,         {3840,2160},  { 320, 240},  1440,  {1920,1080});

    // Transcoding to a resolution not supported by the decoder for the transcoding codec.
    T(true,   true,   {1280, 720},  none,         {3840,2160},  { 320, 240},  1440,  low        );

    #undef T

    finishTest(HasFailure());
}

} // namespace test
} // namespace media
} // namespace nx
