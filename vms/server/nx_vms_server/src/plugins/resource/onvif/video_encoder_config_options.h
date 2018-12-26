#pragma once

#include <vector>

#include <QSize>
#include <QString>

#include <nx/utils/std/optional.h>

#include <onvif/soapStub.h>

#include "soap_wrapper.h"

/**
 * Server understands only limited number of videocodecs.
 * They are listed in UnderstandableVideoCodec enum.
 * Values are used while sorting configurationOptions, so the order of codecs is meaningful.
 * Desirable codec is the codec with the maximum value.
 */
enum class UnderstandableVideoCodec
{
    NONE,
    JPEG,
    H264,
    H265,
    Desirable = H265
};
/*
 FYI: is how video encoders are enumerated in onvif:
 enum class onvifXsd__VideoEncodingMimeNames
 { JPEG = 0, MPV4_ES = 1, H264 = 2, H265 = 3 };
*/

UnderstandableVideoCodec VideoCodecFromString(const std::string& name);
std::string VideoCodecToString(UnderstandableVideoCodec codec);

std::optional<onvifXsd__VideoEncodingProfiles> VideoEncoderProfileFromString(const std::string& name);
std::string VideoEncoderProfileToString(onvifXsd__VideoEncodingProfiles profile);

std::optional<onvifXsd__H264Profile> H264ProfileFromString(const std::string& name);
std::string H264ProfileToString(onvifXsd__H264Profile profile);

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
    UnderstandableVideoCodec encoder = UnderstandableVideoCodec::NONE;
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

