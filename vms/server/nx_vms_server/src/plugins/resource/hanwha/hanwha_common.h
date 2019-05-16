#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms::server {
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
    archive,
    preview,
    fileExport,
    count
};

//TODO: #dmishin consider using Fusion instead of custom methods.

enum class HanwhaDeviceType
{
    unknown,
    nwc,
    nvr,
    dvr,
    encoder,
    decoder,
    hybrid
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(HanwhaDeviceType);

static const int kHanwhaBlockedHttpCode = 490;
static const int kHanwhaInvalidParameterHttpCode = 602;

static const QString kHanwhaManufacturerName("Hanwha Techwin");
static const int kHanwhaInvalidProfile = -1;
static const int kHanwhaInvalidGovLength = -1;
static const int kHanwhaInvalidFps = -1;
static const int kHanwhaInvalidBitrate = -1;
static const int kHanwhaInvalidChannel = -1;
static const int kHanwhaDefaultOverlappedId = 0;
static const int kHanwhaMaxSecondaryStreamArea = 1024 * 768;
static const int kHanwhaDefaultMaxPresetNumber = 1000;
static const int kHanwhaProfileNameDefaultMaxLength = 12;

static const QString kHanwhaDateTimeFormat("yyyy-MM-dd hh:mm:ss");
static const QString kHanwhaUtcDateTimeFormat("yyyy-MM-ddThh:mm:ssZ");

static const QString kHanwhaPrimaryNxProfileSuffix = "Primary";
static const QString kHanwhaSecondaryNxProfileSuffix = "Secondary";

static const QString kHanwhaTrue = "True";
static const QString kHanwhaFalse = "False";

static const QString kHanwhaMjpeg = "MJPEG";
static const QString kHanwhaH264 = "H264";
static const QString kHanwhaHevc = "H265";

static const QString kHanwhaCbr = "CBR";
static const QString kHanwhaVbr = "VBR";

static const QString kHanwhaFrameRatePriority = "FrameRate";
static const QString kHanwhaCompressionLevelPriority = "CompressionLevel";

static const QString kHanwhaCabac = "CABAC";
static const QString kHanwhaCavlc = "CAVLC";

static const QString kHanwhaBaselineProfile = "Baseline";
static const QString kHanwhaMainProfile = "Main";
static const QString kHanwhaHighProfile = "High";

static const QString kHanwhaLiveMediaType = "Live";
static const QString kHanwhaSearchMediaType = "Search";
static const QString kHanwhaBackupMediaType = "Backup";

static const QString kHanwhaIframeOnlyMode = "IframeOnly";
static const QString kHanwhaFullMode = "Full";

static const QString kHanwhaRtpUnicast = "RTPUnicast";
static const QString kHanwhaRtpMulticast = "RTPMulticast";
static const QString kHanwhaRtpMulticastEnable = "RTPMulticastEnable";
static const QString kHanwhaRtpMulticastAddress = "RTPMulticastAddress";
static const QString kHanwhaRtpMulticastPort = "RTPMulticastPort";
static const QString kHanwhaRtpMulticastTtl = "RTPMulticastTTL";

static const QString kHanwhaPcClient = "PC";
static const QString kHanwhaMobileClient = "Mobile";

static const QString kHanwhaAll = "All";

static const QString kHanwhaChannelPropertyTemplate = "Channel.%1";
static const QString kHanwhaChannelProperty = "Channel";
static const QString kHanwhaIsFixedProfileProperty = "IsFixedProfile";
static const QString kHanwhaProfileNameProperty = "Name";
static const QString kHanwhaProfileNumberProperty = "Profile";
static const QString kHanwhaEncodingTypeProperty = "EncodingType";
static const QString kHanwhaFrameRateProperty = "FrameRate";
static const QString kHanwhaBitrateProperty = "Bitrate";
static const QString kHanwhaResolutionProperty = "Resolution";
static const QString kHanwhaAudioInputEnableProperty = "AudioInputEnable";
static const QString kHanwhaMediaTypeProperty = "MediaType";
static const QString kHanwhaStreamingModeProperty("Mode");
static const QString kHanwhaStreamingTypeProperty = "StreamType";
static const QString kHanwhaTransportProtocolProperty = "TransportProtocol";
static const QString kHanwhaRtspOverHttpProperty = "RTSPOverHTTP";
static const QString kHanwhaClientTypeProperty = "ClientType";
static const QString kHanwhaUriProperty = "URI";
static const QString kHanwhaPanProperty = "Pan";
static const QString kHanwhaTiltProperty = "Tilt";
static const QString kHanwhaZoomProperty = "Zoom";
static const QString kHanwhaFocusProperty = "Focus";
static const QString kHanwhaNormalizedSpeedProperty = "NormalizedSpeed";
static const QString kHanwhaPtzQueryProperty = "Query";
static const QString kHanwhaPresetNumberProperty = "Preset";
static const QString kHanwhaPresetNameProperty = "Name";
static const QString kHanwhaHorizontalFlipProperty = "HorizontalFlipEnable";
static const QString kHanwhaVerticalFlipProperty = "VerticalFlipEnable";
static const QString kHanwhaRotationProperty = "Rotate";

static const QString kHanwhaMaxFpsProperty = "MaxFPS";
static const QString kHanwhaDefaultFpsProperty = "DefaultFPS";
static const QString kHanwhaMaxCbrBitrateProperty = "MaxCBRTargetBitrate";
static const QString kHanwhaMinCbrBitrateProperty = "MinCBRTargetBitrate";
static const QString kHanwhaDefaultCbrBitrateProperty = "DefaultCBRTargetBitrate";
static const QString kHanwhaMaxVbrBitrateProperty = "MaxVBRTargetBitrate";
static const QString kHanwhaMinVbrBitrateProperty = "MinVBRTargetBitrate";
static const QString kHanwhaDefaultVbrBitrateProperty = "DefaultVBRTargetBitrate";
static const QString kHanwhaChannelIdListProperty = "ChannelIDList";
static const QString kHanwhaFromDateProperty = "FromDate";
static const QString kHanwhaToDateProperty = "ToDate";
static const QString kHanwhaOverlappedIdProperty = "OverlappedID";
static const QString kHanwhaResultsInUtcProperty = "ResultsInUTC";
static const QString kHanwhaRecordingTypeProperty = "Type";
static const QString kHanwhaSequenceIdProperty = "SunapiSeqId";

static const QString kHanwhaNearFocusMove = "Near";
static const QString kHanwhaFarFocusMove = "Far";
static const QString kHanwhaStopFocusMove = "Stop";

static const QString kHanwhaNameAttribute = "name";
static const QString kHanwhaValueAttribute = "value";
static const QString kHanwhaChannelNumberAttribute = "number";

static const QString kHanwhaAttributesNodeName = "attributes";
static const QString kHanwhaGroupNodeName = "group";
static const QString kHanwhaCategoryNodeName = "category";
static const QString kHanwhaChannelNodeName = "channel";
static const QString kHanwhaAttributeNodeName = "attribute";

static const QString kHanwhaCgiNodeName = "cgi";
static const QString kHanwhaCgisNodeName = "cgis";
static const QString kHanwhaSubmenuNodeName = "submenu";
static const QString kHanwhaActionNodeName = "action";
static const QString kHanwhaParameterNodeName = "parameter";
static const QString kHanwhaDataTypeNodeName = "dataType";
static const QString kHanwhaEnumNodeName = "enum";
static const QString kHanwhaCsvNodeName = "csv";
static const QString kHanwhaEnumEntryNodeName = "entry";
static const QString kHanwhaIntegerNodeName = "int";
static const QString kHanwhaBooleanNodeName = "bool";
static const QString kHanwhaStringNodeName = "string";
static const QString kHanwhaFloatNodeName = "float";

static const QString kHanwhaParameterIsRequestAttribute = "request";
static const QString kHanwhaParameterIsResponseAttribute = "response";
static const QString kHanwhaMinValueAttribute = "min";
static const QString kHanwhaMaxValueAttribute = "max";
static const QString kHanwhaTrueValueAttribute = "true";
static const QString kHanwhaFalseValueAttribute = "false";
static const QString kHanwhaFormatInfoAttribute = "formatInfo";
static const QString kHanwhaFormatAttribute = "format";
static const QString kHanwhaMaxLengthAttribute = "maxlen";

static const int kHanwhaConfigurationNotFoundError = 612;

static const QString kHanwhaNormalizedSpeedPtzTrait("NormalizedSpeed");
static const QString kHanwhaHas3AxisPtz("3AxisPTZ");
static const QString kHanwhaSimpleFocusTrait("SimpleFocusTrait");
static const QString kHanwhaAutoFocusTrait("AutoFocusTrait");

static const QString kHanwhaAlternativeZoomTrait("AlternativeZoomTrait");
static const QString kHanwhaAlternativeFocusTrait("AlternativeFocusTrait");
static const QString kHanwhaAlternativePanTrait("AlternativePanTrait");
static const QString kHanwhaAlternativeTiltTrait("AlternativeTiltTrait");
static const QString kHanwhaAlternativeRotateTrait("AlternativeRotateTrait");

static const int kMaxPossibleFps = 1000;

static const QString kHanwhaProxiedIdParamName = "proxiedId";

static const QString kHanwhaDefaultMinimalBypassFirmware = "2.10";
static const QString kHanwhaBypassOverrideParameterName = "bypassOverride";
static const QString kHanwhaMinimalBypassFirmwareParameterName = "bypassFirmware";

// TODO: #dmishin get rid of the properties below and move Hanwha driver to the standard
// profile configuration mechanism.
static const QString kPrimaryStreamResolutionParamName = "primaryStreamResolution";
static const QString kPrimaryStreamCodecParamName = "primaryStreamCodec";
static const QString kPrimaryStreamCodecProfileParamName = "primaryStreamCodecProfile";
static const QString kPrimaryStreamGovLengthParamName = "primaryStreamGovLength";
static const QString kPrimaryStreamBitrateControlParamName = "primaryStreamBitrateControl";
static const QString kPrimaryStreamBitrateParamName = "primaryStreamBitrate";
static const QString kPrimaryStreamEntropyCodingParamName = "primaryStreamEntropyCoding";
static const QString kPrimaryStreamFpsParamName = "primaryStreamFps";

static const QString kSecondaryStreamResolutionParamName = "secondaryStreamResolution";
static const QString kSecondaryStreamCodecParamName = "secondaryStreamCodec";
static const QString kSecondaryStreamCodecProfileParamName = "secondaryStreamCodecProfile";
static const QString kSecondaryStreamGovLengthParamName = "secondaryStreamGovLength";
static const QString kSecondaryStreamBitrateControlParamName = "secondaryStreamBitrateControl";
static const QString kSecondaryStreamBitrateParamName = "secondaryStreamBitrate";
static const QString kSecondaryStreamEntropyCodingParamName = "secondaryStreamEntropyCoding";
static const QString kSecondaryStreamFpsParamName = "secondaryStreamFps";

} // namespace plugins
} // namespace vms::server
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::plugins::HanwhaDeviceType, (lexical));
