#include <gtest/gtest.h>

#include <nx/utils/log/log.h>

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

namespace nx {
namespace media {
namespace test {

namespace {

static std::string qSizeToStdString(const QSize& size)
{
    return lit("%1 x %2").arg(size.width()).arg(size.height()).toLatin1().constData();
}

//-------------------------------------------------------------------------------------------------
// Test/mock classes.

class TestPlayer: public Player
{
public:
    TestPlayer(QnArchiveStreamReader* archiveReader, QnCommonModule* commonModule):
        m_module(commonModule)
    {
        testSetOwnedArchiveReader(archiveReader);
    }

    using Player::testSetCamera;

protected:
    virtual QnCommonModule* commonModule() const override
    {
        return m_module;
    }

private:
    QnCommonModule* m_module;
};

class MockVideoDecoder: public AbstractVideoDecoder
{
public:
    MockVideoDecoder(const ResourceAllocatorPtr& /*allocator*/, const QSize& /*resolution*/)
    {
    }

    virtual ~MockVideoDecoder() {}

    static bool isCompatible(const AVCodecID codec, const QSize& resolution)
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
    virtual Qn::LicenseType licenseType() const override { return m_cameraType; }

protected:
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
        m_module->resourcePool()->clear(); //< Just in case.
        m_module->resourcePool()->addResource(m_server);
        m_module->resourcePool()->addResource(m_camera);
    }

    ~PlayerSetQualityTest()
    {
        m_module->resourcePool()->clear();
    }

    // Can be called multiple times.
    void test(
        int sourceCodeLineNumber,
        const char* sourceCodeLineString,
        int channelCount,
        bool clientSupportsTranscoding,
        bool serverSupportsTranscoding,
        QSize maxTranscodingResolution,
        QSize maxDecoderResolution,
        QSize highStreamResolution,
        QSize lowStreamResolution,
        int videoQuality,
        QSize expectedQuality)
    {
        NX_LOG(lit("[TEST] ====================================================================="),
            cl_logINFO);
        NX_LOG(lit("[TEST] line %1: %2\n").arg(sourceCodeLineNumber).arg(sourceCodeLineString),
            cl_logINFO);

        VideoDecoderRegistry::instance()->setTranscodingEnabled(clientSupportsTranscoding);
        m_server->setSupportsTranscoding(serverSupportsTranscoding);
        MockVideoDecoder::s_maxResolution = maxDecoderResolution;
        MockVideoDecoder::s_transcodingCodec = m_archiveReader->getTranscodingCodec();
        MockVideoDecoder::s_maxTranscodedResolution = maxTranscodingResolution;
        m_camera->setStreams(highStreamResolution, lowStreamResolution);
        m_camera->setChannelCount(channelCount);

        m_calledSetQuality = false;

        // Reset stored Video Quality value to guarantee that our new value will be processed.
        m_player->testSetCamera(QnResourcePtr(nullptr));
        m_player->setVideoQuality(-1);
        m_player->testSetCamera(m_camera);

        m_player->setVideoQuality(videoQuality); //< Calls handleSetQuality().

        EXPECT_TRUE(m_calledSetQuality) << "setQuality() has not been called by Player";
        if (m_calledSetQuality)
        {
            if (expectedQuality == media_player_quality_chooser::kQualityLow)
            {
                checkActualValues(MEDIA_Quality_Low, QSize());
            }
            else if (expectedQuality == media_player_quality_chooser::kQualityHigh)
            {
                checkActualValues(MEDIA_Quality_High, QSize());
            }
            else if (expectedQuality == media_player_quality_chooser::kQualityLowIframesOnly)
            {
                checkActualValues(MEDIA_Quality_LowIframesOnly, QSize());
            }
            else
            {
                checkActualValues(MEDIA_Quality_CustomResolution,
                    QSize(/*width*/ 0, expectedQuality.height())); //< Player sets "auto" width.
            }
        }
    }

private:
    void checkActualValues(MediaQuality expectedMediaQuality, QSize expectedResolution)
    {
        // ASSERT_EQ not used to properly convert values to strings.

        if (expectedMediaQuality != m_actualQuality)
        {
            ADD_FAILURE() << "MediaQuality: "
                << "expected " << QnLexical::serialized(expectedMediaQuality).toUtf8().constData()
                << ", actual " << QnLexical::serialized(m_actualQuality).toUtf8().constData();
        }

        if (expectedResolution != m_actualResolution)
        {
            ADD_FAILURE() << "Resolution: "
                << "expected " << qSizeToStdString(expectedResolution)
                << ", actual " << qSizeToStdString(m_actualResolution);
        }
    }

    void handleSetQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution)
    {
        EXPECT_TRUE(fastSwitch);

        EXPECT_FALSE(m_calledSetQuality) << "setQuality() called more than once.";
        m_calledSetQuality = true;

        NX_LOG(lit("[TEST] setQuality(%1, fastSwitch: %2, %3 x %4);")
            .arg(QnLexical::serialized(quality))
            .arg(fastSwitch)
            .arg(resolution.width())
            .arg(resolution.height()), cl_logINFO);

        m_actualQuality = quality;
        m_actualResolution = resolution;
    }

private:
    QnStaticCommonModule m_staticCommon;
    std::unique_ptr<QnCommonModule> m_module{new QnCommonModule(false, nx::core::access::Mode::direct)};

    QnSharedResourcePointer<MockServer> m_server{new MockServer(m_module.get())};

    QnSharedResourcePointer<MockCamera> m_camera{new MockCamera(m_server->getId())};

    MockArchiveStreamReader* m_archiveReader{new MockArchiveStreamReader(
        m_camera,
        [this](MediaQuality quality, bool fastSwitch, const QSize& resolution)
        {
            handleSetQuality(quality, fastSwitch, resolution);
        }
    )};

    std::unique_ptr<TestPlayer> m_player{new TestPlayer(m_archiveReader, m_module.get())};

    bool m_calledSetQuality;

    // Test results.
    MediaQuality m_actualQuality;
    QSize m_actualResolution;
};

} // namespace

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

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(NxMediaPlayerTest, SetQuality)
{
    PlayerSetQualityTest test;

    static const QSize none{};
    static const QSize high{media_player_quality_chooser::kQualityHigh};
    static const QSize low{media_player_quality_chooser::kQualityLow};
    static const QSize lowIframes{media_player_quality_chooser::kQualityLowIframesOnly};
    static const Player::VideoQuality hi{Player::HighVideoQuality};
    static const Player::VideoQuality lo{Player::LowVideoQuality};
    static const Player::VideoQuality li{Player::LowIframesOnlyVideoQuality};
    static const bool no = false;
    static const bool yes = true;
    #define T(...) test.test(__LINE__, #__VA_ARGS__, __VA_ARGS__)

    // NOTE: kMaxTranscodingResolution is defined in Player private as 1920 x 1080.

  //     TranscSup                                                       Quality
  // #Ch Cli  Ser  MaxTranscRes MaxDecodRes  HiStreamRes  LoStreamRes  hi/lo Expected

    // High stream requested.
    T(1, no,  no,  none,        {1920,1080}, {1920,1080}, { 320, 240},   hi, high       );
    T(1, no,  no,  none,        {1920,1080}, {1920,1080}, { 320, 240}, 1080, high       );
    T(1, no,  no,  none,        {1920,1080}, {1920,1080}, { 320, 240},   hi, high       );
    T(1, no,  no,  none,        { 640, 480}, {1920,1080}, { 320, 240},   hi, low        );
    T(1, yes, yes, none,        { 640, 480}, {1280, 720}, { 320, 240},   hi, low        );
    T(1, yes, yes, none,        {1920,1080}, {2560,1440}, { 320, 240},   hi, {1920,1080});
    T(1, yes, yes, none,        {1920,1080}, none,        { 320, 240},   hi, low        );

    // Low stream requested.
    T(1, yes, yes, none,        {1920,1080}, {1920,1080}, { 320, 240},   lo, low        );
    T(1, yes, yes, none,        {1920,1080}, {1920,1080}, { 320, 240},  240, low        );

    // Invalid Video Quality.
    T(1, yes, yes, none,        none,        {1920,1080}, { 640, 480}, -113, low        );

    // Transcoding requested and is available.
    T(1, yes, yes, none,        none,        none,        { 640, 480},  360, { 480, 360});

    // Transcoding requested but is not available.
    T(1, no,  no,  none,        none,        none,        { 640, 480},  360, low        );
    T(1, yes, no,  none,        none,        none,        { 640, 480},  360, low        );
    T(1, no,  yes, none,        none,        none,        { 640, 480},  360, low        );

    // Transcoding to the requested resolution which is within limits.
    T(1, yes, yes, none,        {1920,1080}, {1920,1080}, { 320, 240},  720, {1280, 720});

    // Transcoding to a resolution limited by MaxDecoderRes.
    T(1, yes, yes, none,        {1280, 720}, {2560,1440}, { 320, 240}, 1080, {1280, 720});

    // Transcoding to a resolution limited by kMaxTranscodingResolution.
    T(1, yes, yes, none,        none,        {3840,2160}, { 320, 240}, 1440, {1920,1080});

    // Transcoding to a resolution not supported by the decoder for the transcoding codec.
    T(1, yes, yes, {1280, 720}, none,        {3840,2160}, { 320, 240}, 1440, low        );

    // Panoramic camera: transcoding is supported, thus, should be used.
    T(4, yes, yes, none,        {1920,1080}, {1920,1080}, { 320, 240},   hi, {1920,1080});

    // Panoramic camera: transcoding is not supported, thus, low quality should be chosen.
    T(4, no,  no,  none,        {1920,1080}, {1920,1080}, { 320, 240},   hi, low        );

    // Low stream I-frames only requested.
    T(1, yes, yes, none,        {1920,1080}, {1920,1080}, { 320, 240},   li, lowIframes );

    #undef T
}

} // namespace test
} // namespace media
} // namespace nx
