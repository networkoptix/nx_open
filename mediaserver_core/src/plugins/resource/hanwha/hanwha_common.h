#pragma once

#if defined(ENABLE_HANWHA)

namespace nx {
namespace mediaserver_core {
namespace plugins {

enum class HanwhaMediaType
{
    undefined,
    live,
    search,
    backup
};

enum class HanwhaStreamingMode
{
    undefined,
    iframeOnly,
    full
};

enum class HanwhaStreamingType
{
    undefined,
    rtpUnicast,
    rtpMulticast
};

enum class HanwhaTransportProtocol
{
    undefined,
    tcp,
    udp
};

enum class HanwhaClientType
{
    undefined,
    pc,
    mobile
};

enum class HanwhaSessionType
{
    undefined,
    live,
    playback,
    backup
};

//TODO: #dmishin consider using Fusion instead of custom methods.

static const int kHanwhaBlockedHttpCode = 490;

static const QString kHanwhaManufacturerName("Hanwha Techwin");
static const int kHanwhaInvalidProfile = -1;
static const int kHanwhaInvalidGovLength = -1;
static const int kHanwhaInvalidFps = -1;
static const int kHanwhaInvalidBitrate = -1;
static const int kHanwhaInvalidChannel = -1;
static const int kHanwhaMaxSecondaryStreamArea = 1024 * 768;
static const int kHanwhaDefaultMaxPresetNumber = 1000;
static const int kHanwhaProfileNameMaxLength = 12;

const QString kHanwhaPrimaryNxProfileSuffix = lit("Primary");
const QString kHanwhaSecondaryNxProfileSuffix = lit("Secondary");

static const QString kHanwhaTrue = lit("True");
static const QString kHanwhaFalse = lit("False");

static const QString kHanwhaMjpeg = lit("MJPEG");
static const QString kHanwhaH264 = lit("H264");
static const QString kHanwhaHevc = lit("H265");

static const QString kHanwhaCbr = lit("CBR");
static const QString kHanwhaVbr = lit("VBR");

static const QString kHanwhaFrameRatePriority = lit("FrameRate");
static const QString kHanwhaCompressionLevelPriority = lit("CompressionLevel");

static const QString kHanwhaCabac = lit("CABAC");
static const QString kHanwhaCavlc = lit("CAVLC");

static const QString kHanwhaBaselineProfile = lit("Baseline");
static const QString kHanwhaMainProfile = lit("Main");
static const QString kHanwhaHighProfile = lit("High");

static const QString kHanwhaLiveMediaType = lit("Live");
static const QString kHanwhaSearchMediaType = lit("Search");
static const QString kHanwhaBackupMediaType = lit("Backup");

static const QString kHanwhaIframeOnlyMode = lit("IframeOnly");
static const QString kHanwhaFullMode = lit("Full");

static const QString kHanwhaRtpUnicast = lit("RTPUnicast");
static const QString kHanwhaRtpMulticast = lit("RTPMulticast");

static const QString kHanwhaTcp = lit("TCP");
static const QString kHanwhaUdp = lit("UDP");

static const QString kHanwhaPcClient = lit("PC");
static const QString kHanwhaMobileClient = lit("Mobile");

static const QString kHanwhaChannelPropertyTemplate = lit("Channel.%1");
static const QString kHanwhaChannelProperty = lit("Channel");
static const QString kHanwhaIsFixedProfileProperty = lit("IsFixedProfile");
static const QString kHanwhaProfileNameProperty = lit("Name");
static const QString kHanwhaProfileNumberProperty = lit("Profile");
static const QString kHanwhaEncodingTypeProperty = lit("EncodingType");
static const QString kHanwhaFrameRateProperty = lit("FrameRate");
static const QString kHanwhaBitrateProperty = lit("Bitrate");
static const QString kHanwhaResolutionProperty = lit("Resolution");
static const QString kHanwhaAudioInputEnableProperty = lit("AudioInputEnable");
static const QString kHanwhaMediaTypeProperty = lit("MediaType");
static const QString kHanwhaStreamingModeProperty("Mode");
static const QString kHanwhaStreamingTypeProperty = lit("StreamType");
static const QString kHanwhaTransportProtocolProperty = lit("TransportProtocol");
static const QString kHanwhaRtspOverHttpProperty = lit("RTSPOverHTTP");
static const QString kHanwhaClientTypeProperty = lit("ClientType");
static const QString kHanwhaUriProperty = lit("URI");
static const QString kHanwhaPanProperty = lit("Pan");
static const QString kHanwhaTiltProperty = lit("Tilt");
static const QString kHanwhaZoomProperty = lit("Zoom");
static const QString kHanwhaFocusProperty = lit("Focus");
static const QString kHanwhaNormalizedSpeedProperty = lit("NormalizedSpeed");
static const QString kHanwhaPtzQueryProperty = lit("Query");
static const QString kHanwhaPresetNumberProperty = lit("Preset");
static const QString kHanwhaPresetNameProperty = lit("Name");
static const QString kHanwhaHorizontalFlipProperty = lit("HorizontalFlipEnable");
static const QString kHanwhaVerticalFlipProperty = lit("VerticalFlipEnable");
static const QString kHanwhaRotationProperty = lit("Rotate");

static const QString kHanwhaMaxFpsProperty = lit("MaxFPS");
static const QString kHanwhaDefaultFpsProperty = lit("DefaultFPS");
static const QString kHanwhaMaxCbrBitrateProperty = lit("MaxCBRTargetBitrate");
static const QString kHanwhaMinCbrBitrateProperty = lit("MinCBRTargetBitrate");
static const QString kHanwhaDefaultCbrBitrateProperty = lit("DefaultCBRTargetBitrate");
static const QString kHanwhaMaxVbrBitrateProperty = lit("MaxVBRTargetBitrate");
static const QString kHanwhaMinVbrBitrateProperty = lit("MinVBRTargetBitrate");
static const QString kHanwhaDefaultVbrBitrateProperty = lit("DefaultVBRTargetBitrate");

static const QString kHanwhaNearFocusMove = lit("Near");
static const QString kHanwhaFarFocusMove = lit("Far");
static const QString kHanwhaStopFocusMove = lit("Stop");

static const QString kHanwhaNameAttribute = lit("name");
static const QString kHanwhaValueAttribute = lit("value");
static const QString kHanwhaChannelNumberAttribute = lit("number");

static const QString kHanwhaAttributesNodeName = lit("attributes");
static const QString kHanwhaGroupNodeName = lit("group");
static const QString kHanwhaCategoryNodeName = lit("category");
static const QString kHanwhaChannelNodeName = lit("channel");
static const QString kHanwhaAttributeNodeName = lit("attribute");

static const QString kHanwhaCgiNodeName = lit("cgi");
static const QString kHanwhaCgisNodeName = lit("cgis");
static const QString kHanwhaSubmenuNodeName = lit("submenu");
static const QString kHanwhaActionNodeName = lit("action");
static const QString kHanwhaParameterNodeName = lit("parameter");
static const QString kHanwhaDataTypeNodeName = lit("dataType");
static const QString kHanwhaEnumNodeName = lit("enum");
static const QString kHanwhaCsvNodeName = lit("csv");
static const QString kHanwhaEnumEntryNodeName = lit("entry");
static const QString kHanwhaIntegerNodeName = lit("int");
static const QString kHanwhaBooleanNodeName = lit("bool");
static const QString kHanwhaStringNodeName = lit("string");
static const QString kHanwhaFloatNodeName = lit("float");

static const QString kHanwhaParameterIsRequestAttribute = lit("request");
static const QString kHanwhaParameterIsResponseAttribute = lit("response");
static const QString kHanwhaMinValueAttribute = lit("min");
static const QString kHanwhaMaxValueAttribute = lit("max");
static const QString kHanwhaTrueValueAttribute = lit("true");
static const QString kHanwhaFalseValueAttribute = lit("false");
static const QString kHanwhaFormatInfoAttribute = lit("formatInfo");
static const QString kHanwhaFormatAttribute = lit("format");
static const QString kHanwhaMaxLengthAttribute = lit("maxlen");

static const int kHanwhaConfigurationNotFoundError = 612;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
