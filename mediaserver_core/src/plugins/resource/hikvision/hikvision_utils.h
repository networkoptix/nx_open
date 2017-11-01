#pragma once

#include <set>
#include <vector>

#include <QtCore/QSize>
#include <QtXml/QDomDocument>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/network/http/http_types.h>
#include <nx/network/http/http_client.h>
#include <utils/media/audioformat.h>
#include <common/common_globals.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {
namespace hikvision {

static const std::chrono::milliseconds kHttpTimeout(5000);
static const QString kContentType = lit("application/xml");

const QString kStatusCodeOk = lit("1");
const QString kSubStatusCodeOk = lit("ok");
static const QString kVideoElementTag = lit("Video");
static const QString kTransportElementTag = lit("Transport");
static const QString kChannelRootElementTag = lit("StreamingChannel");
static const QString kOptionsAttribute = lit("opt");

static const QString kVideoCodecTypeTag = lit("videoCodecType");
static const QString kVideoResolutionWidthTag = lit("videoResolutionWidth");
static const QString kVideoResolutionHeightTag = lit("videoResolutionHeight");
static const QString kFixedQualityTag = lit("fixedQuality");
static const QString kMaxFrameRateTag = lit("maxFrameRate");

static const QString kRtspPortNumberTag = lit("rtspPortNo");

static const QString kPrimaryStreamNumber = lit("01");
static const QString kSecondaryStreamNumber = lit("02");

static const QString kCapabilitiesRequestPathTemplate =
lit("/ISAPI/Streaming/channels/%1/capabilities");

static const QString kChannelStreamingPathTemplate = lit("/Streaming/Channels/%1");

static const std::array<QString, 5> kVideoChannelProperties = {
    kVideoCodecTypeTag,
    kVideoResolutionWidthTag,
    kVideoResolutionHeightTag,
    kFixedQualityTag,
    kMaxFrameRateTag};

static const std::map<QString, AVCodecID> kCodecMap = {
    {lit("MJPEG"), AVCodecID::AV_CODEC_ID_MJPEG},
    {lit("MPEG4"), AVCodecID::AV_CODEC_ID_MPEG4},
    {lit("MPNG"), AVCodecID::AV_CODEC_ID_APNG}, //< Not sure about this one.
    {lit("H.264"), AVCodecID::AV_CODEC_ID_H264},
    {lit("H.265"), AVCodecID::AV_CODEC_ID_HEVC}};

struct ChannelCapabilities
{
    std::set<AVCodecID> codecs;
    std::vector<QSize> resolutions;
    std::vector<int> fps;
    std::vector<int> quality;
};

// Intentionally use struct here just in case we need
// some additional channel properties in the future.
struct ChannelProperties
{
    int rtspPortNumber = 554;
};

struct ChannelStatusResponse final
{
    QString id;
    bool enabled = false;
    QString audioCompression;
    QString audioInputType;
    int speakerVolume = 0;
    bool noiseReduce = false;
    int sampleRateKHz = 0;
};

struct OpenChannelResponse final
{
    QString sessionId;
};

struct CommonResponse final
{
    QString statusCode;
    QString statusString;
    QString subStatusCode;
};

QnAudioFormat toAudioFormat(const QString& codecName, int sampleRateKHz);

std::vector<ChannelStatusResponse> parseAvailableChannelsResponse(
    nx_http::StringType message);

boost::optional<ChannelStatusResponse> parseChannelStatusResponse(
    nx_http::StringType message);

boost::optional<OpenChannelResponse> parseOpenChannelResponse(
    nx_http::StringType message);

boost::optional<CommonResponse> parseCommonResponse(
    nx_http::StringType message);

bool parseChannelCapabilitiesResponse(
    const nx::Buffer& response,
    ChannelCapabilities* outCapabilities);

bool parseVideoElement(const QDomElement& videoElement, ChannelCapabilities* outCapabilities);

bool parseCodecList(const QString& raw, std::set<AVCodecID>* outCodecs);

bool parseIntegerList(const QString& raw, std::vector<int>* outIntegerList);

bool makeResolutionList(
    const std::vector<int>& widths,
    const std::vector<int>& heights,
    std::vector<QSize>* outResolutions);

bool parseChannelPropertiesResponse(
    nx::Buffer& response,
    ChannelProperties* outChannelProperties);

bool parseTransportElement(
    const QDomElement& transportElement,
    ChannelProperties* outChannelProperties);

bool doGetRequest(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    nx::Buffer* outBuffer,
    nx_http::StatusCode::Value* outStatusCode = nullptr);

bool doPutRequest(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const nx::Buffer& buffer,
    nx_http::StatusCode::Value* outStatusCode = nullptr);

bool doRequest(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const nx_http::Method::ValueType& method,
    const nx::Buffer* bufferToSend = nullptr,
    nx::Buffer* outResponseBuffer = nullptr,
    nx_http::StatusCode::Value* outStatusCode = nullptr
);

bool tuneHttpClient(nx_http::HttpClient* outHttpClient, const QAuthenticator& auth);

QString buildChannelNumber(Qn::ConnectionRole role, int channelNumber);

bool codecSupported(AVCodecID codecId, const ChannelCapabilities& channelCapabilities);

bool responseIsOk(const boost::optional<CommonResponse>& response);

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
