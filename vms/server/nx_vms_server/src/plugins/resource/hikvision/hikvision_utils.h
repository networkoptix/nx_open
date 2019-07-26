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
namespace vms::server {
namespace plugins {
namespace hikvision {

static const std::chrono::milliseconds kHttpTimeout(10000); //< practice shows: 5000 is not enough
static const QString kContentType = lit("application/xml");

const QString kStatusCodeOk = lit("1");
const QString kSubStatusCodeOk = lit("ok");
static const QString kVideoElementTag = lit("Video");
static const QString kTransportElementTag = lit("Transport");
static const QString kChannelRootElementTag = lit("StreamingChannel");
static const QString kAdminAccessElementTag = lit("AdminAccessProtocolList");
static const QString kAdminAccessProtocolElementTag = lit("AdminAccessProtocol");
static const QString kOptionsAttribute = lit("opt");

static const QString kVideoCodecTypeTag = lit("videoCodecType");
static const QString kVideoResolutionWidthTag = lit("videoResolutionWidth");
static const QString kVideoResolutionHeightTag = lit("videoResolutionHeight");
static const QString kFixedQualityTag = lit("fixedQuality");
static const QString kBitrateControlTypeTag = lit("videoQualityControlType");
static const QString kFixedBitrateTag = lit("constantBitRate");
static const QString kVariableBitrateTag = lit("vbrUpperCap");
static const QString kMaxFrameRateTag = lit("maxFrameRate");

static const QString kRtspPortNumberTag = lit("rtspPortNo");

static const QString kPrimaryStreamNumber = lit("01");
static const QString kSecondaryStreamNumber = lit("02");

static const QString kVariableBitrateValue = lit("VBR");
static const QString kConstantBitrateValue = lit("CBR");

static const QString kCapabilitiesRequestPathTemplate =
    lit("/ISAPI/Streaming/channels/%1/capabilities");

// TODO: Find out if we have to try both paths.
static const QString kChannelStreamingPathTemplate = lit("/Streaming/Channels/%1");
static const QString kIsapiChannelStreamingPathTemplate = lit("/ISAPI/Streaming/channels/%1");

static const int kFpsThreshold = 200;

static const std::array<QString, 6> kVideoChannelProperties = {
    kVideoCodecTypeTag,
    kVideoResolutionWidthTag,
    kVideoResolutionHeightTag,
    kFixedQualityTag,
    kMaxFrameRateTag,
    kFixedBitrateTag
};

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

    // Cameras usually return framerate values multiplied by 100.
    // Some models return "normal" (not multiplied) values.
    // This vector is sorted in descending order, so the first element is the maximum framerate.
    std::vector<int> fpsInDeviceUnits;
    std::vector<int> quality;
    std::pair<int, int> bitrateRange;

    int realMaxFps() const;
};

// Intentionally use struct here just in case we need
// some additional channel properties in the future.
struct ChannelProperties
{
    int rtspPort = 0;
    nx::utils::Url httpUrl;
};

struct ChannelStatusResponse final
{
    QString id;
    bool enabled = false;
    QString audioCompression;
    QString audioInputType;
    int speakerVolume = 0;
    bool noiseReduce = false;
    int sampleRateHz = 0;
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

struct AdminAccess final
{
    boost::optional<int> httpPort;
    boost::optional<int> httpsPort;
    boost::optional<int> rtspPort;
    boost::optional<int> deviceManagementPort;
};

QnAudioFormat toAudioFormat(const QString& codecName, int sampleRateHz);

std::vector<ChannelStatusResponse> parseAvailableChannelsResponse(
    nx::network::http::StringType message);

boost::optional<ChannelStatusResponse> parseChannelStatusResponse(
    nx::network::http::StringType message);

boost::optional<OpenChannelResponse> parseOpenChannelResponse(
    nx::network::http::StringType message);

boost::optional<CommonResponse> parseCommonResponse(
    nx::network::http::StringType message);

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

bool parseAdminAccessResponse(const nx::Buffer& response, AdminAccess* outAdminAccess);

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
    nx::network::http::StatusCode::Value* outStatusCode = nullptr);

bool doPutRequest(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const nx::Buffer& buffer,
    nx::network::http::StatusCode::Value* outStatusCode = nullptr);

bool doRequest(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const nx::network::http::Method::ValueType& method,
    const nx::Buffer* bufferToSend = nullptr,
    nx::Buffer* outResponseBuffer = nullptr,
    nx::network::http::StatusCode::Value* outStatusCode = nullptr
);

bool tuneHttpClient(nx::network::http::HttpClient* outHttpClient, const QAuthenticator& auth);

// Hikvision channel number is in range [1..N]
QString buildChannelNumber(Qn::ConnectionRole role, int hikvisionChannelNumber);

bool codecSupported(AVCodecID codecId, const ChannelCapabilities& channelCapabilities);

bool responseIsOk(const boost::optional<CommonResponse>& response);

} // namespace hikvision
} // namespace plugins
} // namespace vms::server
} // namespace nx
