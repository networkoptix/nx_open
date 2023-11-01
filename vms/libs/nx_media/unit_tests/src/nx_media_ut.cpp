// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/media/abstract_video_decoder.h>
#include <nx/media/media_player.h>
#include <nx/media/media_player_quality_chooser.h>
#include <nx/media/video_decoder_registry.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <utils/media/ffmpeg_initializer.h>

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
            return nx::format("%1 x %2").args(size.width(), size.height());
        return nx::format("%1").arg(size.height());
    }

    if (size == QSize())
        return "<invalid>";

    return nx::format("<invalid[%1 x %2]>").args(size.width(), size.height());
}

static QString qSizeToString(const std::optional<QSize>& size)
{
    if (size)
        return qSizeToString(*size);

    return "<missing>";
}

//-------------------------------------------------------------------------------------------------
// Test/mock classes.

class MockVideoDecoder: public AbstractVideoDecoder
{
public:
    MockVideoDecoder(
        const RenderContextSynchronizerPtr& /*synchronizer*/, const QSize& /*resolution*/)
    {
    }

    virtual ~MockVideoDecoder() {}

    virtual Capabilities capabilities() const override
    {
        return Capability::noCapability;
    }

    static bool isCompatible(
        const AVCodecID codec, const QSize& resolution, bool /*allowOverlay*/, bool /*allowHW*/)
    {
        if (!s_maxResolution.isEmpty()
            && (resolution.width() > s_maxResolution.width()
                || resolution.height() > s_maxResolution.height()))
        {
            NX_INFO(NX_SCOPE_TAG,
                "isCompatible() -> false: resolution %1 x %2 is higher than %3 x %4",
                resolution.width(),
                resolution.height(),
                s_maxResolution.width(),
                s_maxResolution.height());
            return false;
        }

        if (codec == s_transcodingCodec
            && !s_maxTranscodedResolution.isEmpty()
            && (resolution.width() > s_maxTranscodedResolution.width()
                || resolution.height() > s_maxTranscodedResolution.height()))
        {
            NX_INFO(NX_SCOPE_TAG,
                "isCompatible() -> false: "
                    "For transcoding codec %1, resolution %2 x %3 is higher than %4 x %5",
                s_transcodingCodec,
                resolution.width(),
                resolution.height(),
                s_maxTranscodedResolution.width(),
                s_maxTranscodedResolution.height());
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
        VideoFramePtr* /*outDecodedFrame*/) override
    {
        NX_INFO(this, "INTERNAL ERROR: decode() called");
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
    MockServer():
        QnMediaServerResource()
    {
        setIdUnsafe(QnUuid::createUuid());
    }

    void setSupportsTranscoding(bool supportsTranscoding)
    {
        if (supportsTranscoding)
            setServerFlags(getServerFlags() | nx::vms::api::SF_SupportsTranscoding);
        else
            setServerFlags(getServerFlags() & ~nx::vms::api::SF_SupportsTranscoding);
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
        setIdUnsafe(QnUuid::createUuid());
        setParentId(parentId);

        addFlags(Qn::server_live_cam);
    }

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* /*dataProvider*/ = nullptr) override
    {
        return m_videoLayout;
    }

    /**
     * Either resolution can be missing - the corresponding stream will not be created.
     */
    void setStreams(
        const std::optional<QSize>& highResolution, const std::optional<QSize>& lowResolution)
    {
        setProperty(ResourcePropertyKey::kMediaStreams, QString());

        // For mock camera, use a codec that will never match any reasonable transcoding codec.
        static const int kCodec = /*102400*/ AV_CODEC_ID_PROBE;

        const QString highStr = !highResolution
            ? ""
            : nx::format("{\"encoderIndex\":0, \"resolution\":\"%1x%2\", \"codec\":%3}").args(
                highResolution->width(), highResolution->height(), kCodec);
        const QString lowStr = !lowResolution
            ? ""
            : nx::format("{\"encoderIndex\":1, \"resolution\":\"%1x%2\", \"codec\":%3}").args(
                lowResolution->width(), lowResolution->height(), kCodec);
        const QString separator = (lowStr.isEmpty() || highStr.isEmpty()) ? "" : ", ";
        const QString json = "{\"streams\":[" + lowStr + separator + highStr + "]}";

        setProperty(ResourcePropertyKey::kMediaStreams, json);

        const int expectedStreamsCount =
            (int) lowResolution.has_value() + (int) highResolution.has_value();
        if ((size_t) expectedStreamsCount != mediaStreams().streams.size())
        {
            NX_INFO(this, "INTERNAL ERROR: Failed adding camera streams: expected %1, actual %2",
                expectedStreamsCount, mediaStreams().streams.size());
            NX_CRITICAL(false, "[TEST]");
        }

        NX_DEBUG(this, "Camera streams JSON: %1", json);
        debugLogStreams();
    }

    void setChannelCount(int channelCount)
    {
        m_videoLayout->setChannelCount(channelCount);
    }

    virtual nx::vms::api::ResourceStatus getStatus() const override { return nx::vms::api::ResourceStatus::online; }

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
            if (stream.getEncoderIndex() == nx::vms::api::StreamIndex::primary) //< High
            {
                NX_DEBUG(this, "Camera High stream: %1 x %2, AVCodecID %3",
                    stream.getResolution().width(),
                    stream.getResolution().height(),
                    stream.codec);
            }
            else if (stream.getEncoderIndex() == nx::vms::api::StreamIndex::secondary) //< Low
            {
                NX_DEBUG(this, "Camera Low stream: %1 x %2, AVCodecID %3",
                    stream.getResolution().width(),
                    stream.getResolution().height(),
                    stream.codec);
            }
            else
            {
                NX_DEBUG(this, "INTERNAL ERROR: Unexpected camera stream: %1 x %2, AVCodecID %3",
                    stream.getResolution().width(),
                    stream.getResolution().height(),
                    stream.codec);
                NX_CRITICAL(false, "[TEST]");
            }
        }
    }

private:
    std::shared_ptr<MockVideoLayout> m_videoLayout{new MockVideoLayout()};
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
    nx::vms::common::test::Context m_context{nx::core::access::Mode::direct};
    QnSharedResourcePointer<MockServer> m_server{new MockServer()};
    QnSharedResourcePointer<MockCamera> m_camera{new MockCamera(m_server->getId())};
};

class TestCase
{
    PlayerSetQualityTest* executor = nullptr;

public:
    int lineNumber = 0;
    int channelCount = 1;
    bool serverSupportsTranscoding = true;
    QSize maxTranscodingResolution;
    QSize maxDecoderResolution;
    std::optional<QSize> highStreamResolution;
    std::optional<QSize> lowStreamResolution;
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

    TestCase& noTrans()
    {
        serverSupportsTranscoding = false;
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
                expectedQuality = Result(quality,
                    (lowStreamResolution && lowStreamResolution->isValid())
                        ? *lowStreamResolution
                        : QSize());
                break;
            case Player::HighVideoQuality:
                expectedQuality = Result(quality,
                    (highStreamResolution && highStreamResolution->isValid())
                        ? *highStreamResolution
                        : QSize());
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
        return nx::format("Line %1: ", lineNumber).toQString()
            + nx::format("\nChannels: %1", channelCount)
            + nx::format("\nServer transcoding: %1", serverSupportsTranscoding)
            + nx::format("\nMax transcoding resolution: %1",
                qSizeToString(maxTranscodingResolution))
            + nx::format("\nMax decoder resolution: %1", qSizeToString(maxDecoderResolution))
            + nx::format("\nHigh resolution: %1", qSizeToString(highStreamResolution))
            + nx::format("\nLow resolution: %1", qSizeToString(lowStreamResolution))
            + nx::format("\nRequested quality: %1", videoQualityToString(videoQuality))
            + nx::format("\nExpected quality: %1", expectedQuality.toString());
    }
};

PlayerSetQualityTest::PlayerSetQualityTest()
{
    m_context.systemContext()->resourcePool()->clear(); //< Just in case.
    m_server->setUrl("http://localhost:7001");
    m_context.systemContext()->resourcePool()->addResource(m_server);
    m_context.systemContext()->resourcePool()->addResource(m_camera);
}

PlayerSetQualityTest::~PlayerSetQualityTest()
{
    m_context.systemContext()->resourcePool()->clear();
}

void PlayerSetQualityTest::test(const TestCase& testCase)
{
    NX_INFO(this, "\n=====================================================================");
    NX_VERBOSE(this, "%1\n", testCase);

    m_server->setSupportsTranscoding(testCase.serverSupportsTranscoding);
    MockVideoDecoder::s_maxResolution = testCase.maxDecoderResolution;
    MockVideoDecoder::s_transcodingCodec =
        std::make_unique<QnArchiveStreamReader>(m_camera)->getTranscodingCodec();
    MockVideoDecoder::s_maxTranscodedResolution = testCase.maxTranscodingResolution;
    m_camera->setStreams(testCase.highStreamResolution, testCase.lowStreamResolution);
    m_camera->setChannelCount(testCase.channelCount);

    const std::vector<AbstractVideoDecoder*> currentDecoders{};
    media_player_quality_chooser::Params input;
    input.transcodingCodec = MockVideoDecoder::s_transcodingCodec;
    input.liveMode = true;
    input.positionMs = -1;
    input.camera = m_camera;
    input.allowOverlay = true;
    input.currentDecoders = &currentDecoders;

    const auto& result = media_player_quality_chooser::chooseVideoQuality(
        testCase.videoQuality, input);

    if (result != testCase.expectedQuality)
    {
        ADD_FAILURE() << nx::format("Expected: %1, actual: %2\nTest: %3",
            testCase.expectedQuality, result, testCase).toUtf8().constData();
    }
}

class NxMediaPlayerTest: public nx::vms::common::test::ContextBasedTest
{
protected:
    virtual void SetUp()
    {
        ffmpegInitializer = std::make_unique<QnFfmpegInitializer>();

        VideoDecoderRegistry::instance()->reinitialize(); //< Just in case.
        VideoDecoderRegistry::instance()->addPlugin<MockVideoDecoder>("MockVideoDecoder");
    }

    virtual void TearDown()
    {
        VideoDecoderRegistry::instance()->reinitialize();
        ffmpegInitializer.reset();
    }

private:
    std::unique_ptr<QnFfmpegInitializer> ffmpegInitializer;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(NxMediaPlayerTest, SetQuality)
{
    PlayerSetQualityTest playerSetQualityTest;

    static const auto high = Player::HighVideoQuality;
    static const auto low = Player::LowVideoQuality;
    static const auto lowIframes = Player::LowIframesOnlyVideoQuality;
    static const auto unknown = Player::UnknownVideoQuality;

    #define T TestCase().setLineNumber(__LINE__).setExecutor(&playerSetQualityTest)

    // High quality requested.
    T.noTrans().low(320, 240).high(1920, 1080).max(1920, 1080).req(high) >> high;
    T.noTrans().low(320, 240).high(1920, 1080).max(1920, 1080).req(1080) >> high;
    T.noTrans().low(320, 240).high(1920, 1080).max( 640,  480).req(high) >> low;
    T.noTrans()              .high(2560, 1440).max(1920, 1080).req(high) >> unknown;
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(1080) >> high;
    T          .low(320, 240).high(1280,  720).max( 640,  480).req(high) >> low;
    T          .low(320, 240).high(2560, 1440).max(1920, 1080).req(high) >> QSize(1920, 1080);
    T          .low( -1,  -1).high(  -1,   -1).max(1920, 1080).req(high) >> high;

    // Low quality requested.
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(low)  >> low;
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(240)  >> low;
    T          .low( -1,  -1).high(  -1,   -1).max(1920, 1080).req(low)  >> low;

    // Invalid video quality.
    T          .low(320, 240).high(1920, 1080).max(1920, 1080).req(-113) >> unknown;

    // Transcoding requested and is available.
    T          .low(640, 480).high(1920, 1080).max(1920, 1080).req(360)  >> QSize(640, 360);

    // Transcoding requested but is not available.
    T.noTrans().low(640, 480).high(1920, 1080).max(1920, 1080).req(360)  >> low;
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

    // Panoramic camera: transcoding is not supported, but maximum resoltion is not limited.
    T.channels(4).noTrans() // so we must return the source stream.
               .low(320, 240).high(1920, 1080).max(-1, -1).req(high) >> high;

    // Panoramic camera: transcoding is supported, but maximum resoltion is not limited.
    T.channels(4) // so we still must return the source stream.
               .low(320, 240).high(1920, 1080).max(-1, -1).req(high) >> high;

    T                        .high(4096, 2160).max(1920, 1080).req(1080) >> QSize(1920, 1012);

#undef T
}

} // namespace test
} // namespace media
} // namespace nx
