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

// Some cameras (e.g.Hikvision DS-2CD2132F-IWS) barely fit into 6 seconds.
static const std::chrono::milliseconds kHttpTimeout(10000);

static const QString kContentType("application/xml");

const QString kStatusCodeOk("1");
const QString kSubStatusCodeOk("ok");
static const QString kVideoElementTag("Video");
static const QString kTransportElementTag("Transport");
static const QString kChannelRootElementTag("StreamingChannel");
static const QString kAdminAccessElementTag("AdminAccessProtocolList");
static const QString kAdminAccessProtocolElementTag("AdminAccessProtocol");
static const QString kOptionsAttribute("opt");

static const QString kVideoCodecTypeTag("videoCodecType");
static const QString kVideoResolutionWidthTag("videoResolutionWidth");
static const QString kVideoResolutionHeightTag("videoResolutionHeight");
static const QString kFixedQualityTag("fixedQuality");
static const QString kBitrateControlTypeTag("videoQualityControlType");
static const QString kFixedBitrateTag("constantBitRate");
static const QString kVariableBitrateTag("vbrUpperCap");
static const QString kMaxFrameRateTag("maxFrameRate");

static const QString kRtspPortNumberTag("rtspPortNo");

static const QString kPrimaryStreamNumber("01");
static const QString kSecondaryStreamNumber("02");

static const QString kVariableBitrateValue("VBR");
static const QString kConstantBitrateValue("CBR");

static const QString kCapabilitiesRequestPathTemplate("/ISAPI/Streaming/channels/%1/capabilities");

// TODO: Find out if we have to try both paths.
static const QString kChannelStreamingPathTemplate("/Streaming/Channels/%1");
static const QString kIsapiChannelStreamingPathTemplate("/ISAPI/Streaming/channels/%1");

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
    {"MJPEG", AVCodecID::AV_CODEC_ID_MJPEG},
    {"MPEG4", AVCodecID::AV_CODEC_ID_MPEG4},
    {"MPNG", AVCodecID::AV_CODEC_ID_APNG}, //< Not sure about this one.
    {"H.264", AVCodecID::AV_CODEC_ID_H264},
    {"H.265", AVCodecID::AV_CODEC_ID_HEVC}};

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
