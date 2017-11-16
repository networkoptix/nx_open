#include <functional>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/core/access/access_types.h>
#include <common/common_module.h>
#include <common/common_globals.h>
#include <common/static_common_module.h>

#include <nx/media/abstract_video_decoder.h>
#include <nx/media/video_decoder_registry.h>
#include <nx/media/media_player.h>
#include <nx/media/media_player_quality_chooser.h>

#include <nx/streaming/archive_stream_reader.h>
#include <core/resource/camera_resource.h>

#include <core/resource/media_server_resource.h>

#include <common/common_module.h>
#include <utils/media/ffmpeg_initializer.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/serialization/lexical_enum.h>

// TODO: #dklychkov Move mock classes to a separate file.
// TODO: #dklychkov Rename this test to something like quality_chooser_ut.
// TODO: #dklychkov Implement Player test.

namespace nx {
namespace media {
namespace test {

namespace {

static QString qSizeToString(const QSize& size)
{
    if (size.height() >= 0)
    {
        if (size.width() >= 0)
            return lit("%1 x %2").arg(size.width()).arg(size.height());
        return lit("%1").arg(size.height());
    }
    return lit("<invalid>");
}

//-------------------------------------------------------------------------------------------------
// Test/mock classes.

class MockVideoDecoder: public AbstractVideoDecoder
{
public:
    MockVideoDecoder(const ResourceAllocatorPtr& /*allocator*/, const QSize& /*resolution*/)
    {
    }

    virtual ~MockVideoDecoder() {}

    static bool isCompatible(
        const AVCodecID codec, const QSize& resolution, bool /*allowOverlay*/)
    {
        if (!s_maxResolution.isEmpty()
            && (resolution.width() > s_maxResolution.width()
                || resolution.height() > s_maxResolution.height()))
        {
            NX_LOG(lit("[TEST] MockVideoDecoder::isCompatible() -> false: "
                "resolution %1 x %2 is higher than %3 x %4")
                .arg(resolution.width()).arg(resolution.height())
                .arg(s_maxResolution.width()).arg(s_maxResolution.height()), cl_logINFO);
            return false;
        }

        if (codec == s_transcodingCodec
            && !s_maxTranscodedResolution.isEmpty()
            && (resolution.width() > s_maxTranscodedResolution.width()
                || resolution.height() > s_maxTranscodedResolution.height()))
        {
            NX_LOG(lit("[TEST] MockVideoDecoder::isCompatible() -> false: "
                "For transcoding codec %1, resolution %2 x %3 is higher than %4 x %5")
                .arg(s_transcodingCodec)
                .arg(resolution.width()).arg(resolution.height())
                .arg(s_maxTranscodedResolution.width()).arg(s_maxTranscodedResolution.height()),
                cl_logINFO);
            return false;
        }

        return true;
    }

    static QSize maxResolution(const AVCodecID /*codec*/)
    {
        return s_maxResolution;
    }

    virtual int decode(
        const QnConstCompressedVideoDataPtr& /*compressedVideoData*/,
        QVideoFramePtr* /*outDecodedFrame*/) override
    {
        NX_LOG(lit("[TEST] INTERNAL ERROR: VideoDecoder::decode() called"), cl_logINFO);
        return 0;
    }

public:
    // Should be assigned externally.
    static QSize s_maxResolution;
    static AVCodecID s_transcodingCodec;
    static QSize s_maxTranscodedResolution;
};
QSize MockVideoDecoder::s_maxResolution{};
AVCodecID MockVideoDecoder::s_transcodingCodec{AV_CODEC_ID_NONE};
QSize MockVideoDecoder::s_maxTranscodedResolution{};

class MockServer: public QnMediaServerResource
{
public:
    MockServer(QnCommonModule* commonModule):
        QnMediaServerResource(commonModule)
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

class MockVideoLayout: public QnResourceVideoLayout
{
public:
    virtual int channelCount() const override
    {
        return m_channelCount;
    }

    virtual QSize size() const override
    {
        return QSize(channelCount(), 1);
    }

    virtual QPoint position(int /*channel*/) const override
    {
        return QPoint();
    }

    virtual QVector<int> getChannels() const override
    {
        QVector<int> result;
        result.resize(m_channelCount);
        return result;
    }

    void setChannelCount(int channelCount)
    {
        NX_CRITICAL(channelCount >= 1, "[TEST]");
        m_channelCount = channelCount;
    }

private:
    int m_channelCount = 1;
};

//TODO: Use <test_support/resource/camera_resource_stup.h> instead.
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

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* /*dataProvider*/ = nullptr) const override
    {
        return m_videoLayout;
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
            NX_LOG(lit(
                "[TEST] INTERNAL ERROR: Failed adding camera streams: expected %1, actual %2")
                .arg(expectedStreamsCount).arg(mediaStreams().streams.size()), cl_logINFO);
            NX_CRITICAL(false, "[TEST]");
        }

        NX_LOG(lit("[TEST] Camera streams JSON: %1").arg(json), cl_logDEBUG1);
        debugLogStreams();
    }

    void setChannelCount(int channelCount)
    {
        m_videoLayout->setChannelCount(channelCount);
    }

    virtual QString getDriverName() const override { return lit("MockCamera"); }
    virtual void setIframeDistance(int /*frames*/, int /*timems*/) override {}
    virtual Qn::ResourceStatus getStatus() const override { return Qn::Online; }

protected:
    virtual Qn::LicenseType calculateLicenseType() const override { return m_cameraType; }
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override { return nullptr; }

private:
    void debugLogStreams()
    {
        if (!nx::utils::log::isToBeLogged(nx::utils::log::Level::debug))
            return;

        for (const auto& stream: mediaStreams().streams)
        {
            if (stream.encoderIndex == CameraMediaStreamInfo::PRIMARY_STREAM_INDEX) //< High
            {
                NX_LOG(lit("[TEST] Camera High stream: %1 x %2, AVCodecID %3")
                    .arg(stream.getResolution().width()).arg(stream.getResolution().height())
                    .arg(stream.codec), cl_logDEBUG1);
            }
            else if (stream.encoderIndex == CameraMediaStreamInfo::SECONDARY_STREAM_INDEX) //< Low
            {
                NX_LOG(lit("[TEST] Camera Low stream: %1 x %2, AVCodecID %3")
                    .arg(stream.getResolution().width()).arg(stream.getResolution().height())
                    .arg(stream.codec), cl_logDEBUG1);
            }
            else
            {
                NX_LOG(lit(
                    "[TEST] INTERNAL ERROR: Unexpected camera stream: %1 x %2, AVCodecID %3")
                    .arg(stream.getResolution().width()).arg(stream.getResolution().height())
                    .arg(stream.codec), cl_logDEBUG1);
                NX_CRITICAL(false, "[TEST]");
            }
        }
    }

private:
    QSharedPointer<MockVideoLayout> m_videoLayout{new MockVideoLayout()};
    Qn::LicenseType m_cameraType;
};

//-------------------------------------------------------------------------------------------------
// Test mechanism.

class TestCase;

class PlayerSetQualityTest
{
public:
    PlayerSetQualityTest();
    ~PlayerSetQualityTest();

    // Can be called multiple times.
    void test(const TestCase& testCase);

private:
    QnStaticCommonModule m_staticCommon;
    std::unique_ptr<QnCommonModule> m_module{
        new QnCommonModule(false, nx::core::access::Mode::direct)};
    QnSharedResourcePointer<MockServer> m_server{new MockServer(m_module.get())};
    QnSharedResourcePointer<MockCamera> m_camera{new MockCamera(m_server->getId())};
};

class TestCase
{
    PlayerSetQualityTest* executor = nullptr;

public:
    int lineNumber = 0;
    int channelCount = 1;
    bool clientSupportsTranscoding = true;
    bool serverSupportsTranscoding = true;
    QSize maxTranscodingResolution;
    QSize maxDecoderResolution;
    QSize highStreamResolution;
    QSize lowStreamResolution;
    int videoQuality = Player::UnknownVideoQuality;
    media_player_quality_chooser::Result expectedQuality;

    TestCase() {}

    ~TestCase()
    {
        if (executor)
            executor->test(*this);
    }

    TestCase& setLineNumber(int line)
    {
        lineNumber = line;
        return *this;
    }

    TestCase& req(int quality)
    {
        videoQuality = quality;
        return *this;
    }

    TestCase& low(int width, int height)
    {
        lowStreamResolution = QSize(width, height);
        return *this;
    }

    TestCase& high(int width, int height)
    {
        highStreamResolution = QSize(width, height);
        return *this;
    }

    TestCase& channels(int count)
    {
        channelCount = count;
        return *this;
    }

    TestCase& noClientTrans()
    {
        clientSupportsTranscoding = false;
        return *this;
    }

    TestCase& noServerTrans()
    {
        serverSupportsTranscoding = false;
        return *this;
    }

    TestCase& noTrans()
    {
        serverSupportsTranscoding = false;
        clientSupportsTranscoding = false;
        return *this;
    }

    TestCase& maxTrans(int width, int height)
    {
        maxTranscodingResolution = QSize(width, height);
        return *this;
    }

    TestCase& max(int width, int height)
    {
        maxDecoderResolution = QSize(width, height);
        return *this;
    }

    TestCase& setExecutor(PlayerSetQualityTest* executor)
    {
        this->executor = executor;
        return *this;
    }

    TestCase& operator>>(Player::VideoQuality quality)
    {
        using Result = media_player_quality_chooser::Result;

        switch (quality)
        {
            case Player::UnknownVideoQuality:
                expectedQuality = Result();
                break;
            case Player::LowVideoQuality:
            case Player::LowIframesOnlyVideoQuality:
                expectedQuality = Result(quality, lowStreamResolution);
                break;
            case Player::HighVideoQuality:
                expectedQuality = Result(quality, highStreamResolution);
                break;
            default:
                NX_ASSERT(false, "To set custom quality use explicit Result.");
        }

        return *this;
    }

    TestCase& operator>>(const QSize& resolution)
    {
        expectedQuality = media_player_quality_chooser::Result(
            Player::CustomVideoQuality, resolution);

        return *this;
    }

    QString toString() const
    {
        return lit("Line %1: ").arg(lineNumber)
            + lit("\nChannels: %1").arg(channelCount)
            + lit("\nClient transcoding: %1").arg(clientSupportsTranscoding)
            + lit("\nServer transcoding: %1").arg(serverSupportsTranscoding)
            + lit("\nMax transcoding resolution: %1").arg(qSizeToString(maxTranscodingResolution))
            + lit("\nMax decoder resolution: %1").arg(qSizeToString(maxDecoderResolution))
            + lit("\nHigh resolution: %1").arg(qSizeToString(highStreamResolution))
            + lit("\nLow resolution: %1").arg(qSizeToString(lowStreamResolution))
            + lit("\nRequested quality: %1")
                .arg(QnLexical::serialized(static_cast<Player::VideoQuality>(videoQuality)))
            + lit("\nExpected quality: %1").arg(expectedQuality.toString());
    }
};

PlayerSetQualityTest::PlayerSetQualityTest()
{
    m_module->resourcePool()->clear(); //< Just in case.
    m_module->resourcePool()->addResource(m_server);
    m_module->resourcePool()->addResource(m_camera);
}

PlayerSetQualityTest::~PlayerSetQualityTest()
{
    m_module->resourcePool()->clear();
}

void PlayerSetQualityTest::test(const TestCase& testCase)
{
    NX_LOG(lit("[TEST] ====================================================================="),
        cl_logINFO);
    NX_LOG(lit("[TEST] %1\n").arg(testCase.toString()), cl_logDEBUG2);

    VideoDecoderRegistry::instance()->setTranscodingEnabled(
        testCase.clientSupportsTranscoding);
    m_server->setSupportsTranscoding(testCase.serverSupportsTranscoding);
    MockVideoDecoder::s_maxResolution = testCase.maxDecoderResolution;
    MockVideoDecoder::s_transcodingCodec =
        std::make_unique<QnArchiveStreamReader>(m_camera)->getTranscodingCodec();
    MockVideoDecoder::s_maxTranscodedResolution = testCase.maxTranscodingResolution;
    m_camera->setStreams(testCase.highStreamResolution, testCase.lowStreamResolution);
    m_camera->setChannelCount(testCase.channelCount);

    const auto& result = media_player_quality_chooser::chooseVideoQuality(
        MockVideoDecoder::s_transcodingCodec,
        testCase.videoQuality,
        true,
        -1,
        m_camera,
        true);

    if (result != testCase.expectedQuality)
    {
        ADD_FAILURE() <<
            lit("Expected: %1, actual: %2\nTest: %3")
                .arg(testCase.expectedQuality.toString(), result.toString())
                .arg(testCase.toString())
                .toUtf8().constData();
    }
}

class NxMediaPlayerTest: public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_staticCommon.reset(new QnStaticCommonModule());
        // Init singletons.
        m_common.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
        m_common->setModuleGUID(QnUuid::createUuid());
        m_common->store(new QnFfmpegInitializer());

        VideoDecoderRegistry::instance()->reinitialize(); //< Just in case.
        VideoDecoderRegistry::instance()->addPlugin<MockVideoDecoder>();
    }

    virtual void TearDown()
    {
        m_common.reset();
        m_staticCommon.reset();
        VideoDecoderRegistry::instance()->reinitialize();
    }

private:
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnCommonModule> m_common;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(NxMediaPlayerTest, SetQuality)
{
    PlayerSetQualityTest qualityTest;

    static const auto high = Player::HighVideoQuality;
    static const auto low = Player::LowVideoQuality;
    static const auto lowIframes = Player::LowIframesOnlyVideoQuality;
    static const auto unknown = Player::UnknownVideoQuality;

    #define T TestCase().setLineNumber(__LINE__).setExecutor(&qualityTest)

    // High quality requested.
    T.noTrans().low(320, 240).high(1920, 1080).max(1920, 1080).req(high) >> high;
    T.noTrans().low(320, 240).high(1920, 1080).max(1920, 1080).req(1080) >> high;
    T.noTrans().low(320, 240).high(1920, 1080).max(640, 480)  .req(high) >> low;
    T.noTrans()              .high(2560, 1440).max(1920, 1080).req(high) >> unknown;
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(1080) >> high;
    T          .low(320, 240).high(1280, 720) .max(640, 480)  .req(high) >> low;
    T          .low(320, 240).high(2560, 1440).max(1920, 1080).req(high) >> QSize(1920, 1080);

    // Low quality requested.
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(low)  >> low;
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(240)  >> low;

    // Invalid video quality.
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(-113) >> unknown;

    // Transcoding requested and is available.
    T          .low(640, 480).high(1920, 1080).max(1920, 1080).req(360)  >> QSize(640, 360);

    // Transcoding requested but is not available.
    T.noTrans().low(640, 480).high(1920, 1080).max(1920, 1080).req(360)  >> low;
    T.noClientTrans()
               .low(640, 480).high(1920, 1080).max(1920, 1080).req(360)  >> low;
    T.noServerTrans()
               .low(640, 480).high(1920, 1080).max(1920, 1080).req(360)  >> low;

    T.noTrans()              .high(1920, 1080).max(1920, 1080).req(360)  >> high;
    T.noTrans()              .high(2560, 1440).max(1920, 1080).req(360)  >> unknown;
    T.noTrans().low(320, 240).high(2560, 1440).max(1920, 1080).req(360)  >> low;

    // Transcoding to the requested resolution which is within limits.
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(720)  >> QSize(1280, 720);

    // Transcoding to a resolution limited by MaxDecoderRes.
    T          .low(320, 240).high(2560, 1440).max(1280, 720) .req(1080) >> QSize(1280, 720);

    // Transcoding to a resolution limited by kMaxTranscodingResolution.
    T          .low(320, 240).high(3840, 2160).max(3840, 2160).req(1440) >> QSize(1920, 1080);

    // Transcoding to a resolution not supported by the decoder for the transcoding codec.
    T.maxTrans(1280, 720)
               .low(320, 240).high(3840, 2160).max(3840, 2160).req(1440) >> high;

    // Low stream I-frames only requested.
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(lowIframes) >> lowIframes;

    // Low stream I-frames only requested but low stream is not available.
    T                        .high(1920, 1080).max(1920, 1080).req(lowIframes) >> unknown;

    // Panoramic camera: transcoding is supported, thus, should be used.
    T.channels(4)
               .low(320, 240).high(1920, 1080).max(1920, 1080).req(high) >> QSize(1920, 1080);

    T.channels(4).noTrans()
               .low(320, 240).high(1920, 1080).max(1920, 1080).req(high) >> low;

    #undef T
}

} // namespace test
} // namespace media
} // namespace nx
