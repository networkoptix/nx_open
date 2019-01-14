#pragma once

#include <vector>

#include <QSize>
#include <QString>

#include <nx/utils/std/optional.h>

#include <onvif/soapStub.h>

#include "soap_wrapper.h"

/**
 * Server supports only limited number of videocodecs.
 * They are all listed in SupportedVideoCodecFlavor enum.
 * Values are used while sorting configurationOptions, so the order of codecs is meaningful.
 * Desirable codec is the codec with the maximum value.
 */
enum class SupportedVideoCodecFlavor
{
    NONE,
    JPEG,
    H264,
    H265,
    Default = JPEG,
    Desirable = H265
};
/*
 FYI: this is how video encoders are enumerated in onvif:
 enum class onvifXsd__VideoEncodingMimeNames
 { JPEG = 0, MPV4_ES = 1, H264 = 2, H265 = 3 };
*/

/**
 * Transfer video codec string to enum and backward.
 * The string constants for video codec names are taken from onvid.xsd schema [version="18.06"]
 * (https://www.onvif.org/ver10/schema/onvif.xsd)
 * form the <xs:simpleType name="VideoEncodingMimeNames"> type description:
 * "JPEG", "H264", "H265" - official video codecs names in onvif.
 * There is one more name mentioned there - MPV4-ES - but our software doesn't support it.
 */
SupportedVideoCodecFlavor supportedVideoCodecFlavorFromOnvifString(const std::string& name);
std::string supportedVideoCodecFlavorToOnvifString(SupportedVideoCodecFlavor codec);

/**
 * Transfer video codec string to enum and backward.
 * The string constants for video codec names are that are used inside vms:
 * "MJPEG", "H264", "H265".
 */
SupportedVideoCodecFlavor supportedVideoCodecFlavorFromVmsString(const std::string& name);
std::string supportedVideoCodecFlavorToVmsString(SupportedVideoCodecFlavor codec);

/**
 * Transfer h264 profile string to enum and backward.
 * The string constants for profiles are taken from onvid.xsd schema [version="18.06"]
 * (https://www.onvif.org/ver10/schema/onvif.xsd)
 * form the <xs:simpleType name="H264Profile"> type description.
 * All other values are not correct and can not pass xsd validation.
 */
std::optional<onvifXsd__H264Profile> h264ProfileFromOnvifString(const std::string& name);
std::string h264ProfileToOnvifString(onvifXsd__H264Profile profile);

/**
 * Transfer h264/h265 profile string to enum and backward.
 * The string constants for profiles are taken from onvid.xsd schema [version="18.06"]
 * (https://www.onvif.org/ver10/schema/onvif.xsd)
 * form the <xs:simpleType name="VideoEncodingProfiles"> type description.
 * All other values are not correct and can not pass xsd validation.
 */
std::optional<onvifXsd__VideoEncodingProfiles> videoEncodingProfilesFromOnvifString(const std::string& name);
std::string VideoEncodingProfilesToString(onvifXsd__VideoEncodingProfiles profile);

// This class should be moved somewhere into utils.
template<class T>
struct Range
{
    T low = 0;
    T high = 0;
    Range() = default;
    Range(T low, T high) : low(low), high(high) {}
};

// this is a wrapper over onvifXsd__VideoEncoder2ConfigurationOptions class
struct VideoEncoderConfigOptions
{
    //required elements
    int mediaWebserviseVersion = 0; // 0 - undefined, 1 - Media1, 2 - Media 2
    SupportedVideoCodecFlavor encoder = SupportedVideoCodecFlavor::NONE;
    Range<float> qualityRange;
    Range<int> bitrateRange;
    std::optional<bool> constantBitrateSupported;
    std::optional<Range<int>> govLengthRange;
    std::vector<QSize> resolutions;
    std::vector<int> frameRates; //< may be empty
    std::vector<onvifXsd__VideoEncodingProfiles> encoderProfiles; //< may be empty

    VideoEncoderConfigOptions() = default;
    VideoEncoderConfigOptions(const onvifXsd__JpegOptions& options);
    VideoEncoderConfigOptions(const onvifXsd__Mpeg4Options& options);
    VideoEncoderConfigOptions(const onvifXsd__H264Options& options);
    VideoEncoderConfigOptions(const onvifXsd__VideoEncoder2ConfigurationOptions& options);
    bool operator < (const VideoEncoderConfigOptions& rhs) const { return encoder < rhs.encoder; }
};

class VideoEncoderConfigOptionsList: public std::vector<VideoEncoderConfigOptions>
{
public:
    VideoEncoderConfigOptionsList() = default;
    VideoEncoderConfigOptionsList(
        const _onvifMedia__GetVideoEncoderConfigurationOptionsResponse& response);
    VideoEncoderConfigOptionsList(
        const _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse& response);
};
