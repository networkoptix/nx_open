#pragma once

#include <vector>

#include <QSize>
#include <QString>

#include <nx/utils/std/optional.h>

#include <onvif/soapStub.h>

#include "soap_wrapper.h"

//class onvifXsd__AudioEncoderConfigurationOption;
//class onvifXsd__VideoSourceConfigurationOptions;
//class onvifXsd__VideoEncoderConfigurationOptions;
//class onvifXsd__VideoEncoderConfiguration;
//class oasisWsnB2__NotificationMessageHolderType;
//class onvifXsd__VideoSourceConfiguration;
//class onvifXsd__EventCapabilities;
//
//class onvifXsd__VideoEncoder2ConfigurationOptions;
//class _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse;
//
//typedef onvifXsd__AudioEncoderConfigurationOption AudioOptions;
//typedef onvifXsd__VideoSourceConfigurationOptions VideoSrcOptions;
//typedef onvifXsd__VideoEncoderConfigurationOptions VideoOptions;
//typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;
//typedef onvifXsd__VideoSourceConfiguration VideoSource;

enum class VIDEO_CODEC //< The order is used in sorting.
{
    NONE,
    JPEG,
    MPEG4,
    H265,
    H264,
    COUNT
};
/*
 FYI: is how video encoders are enumerated in onvif:
 enum class onvifXsd__VideoEncodingMimeNames
 { JPEG = 0, MPV4_ES = 1, H264 = 2, H265 = 3 };
*/

VIDEO_CODEC VideoCodecFromString(const std::string& name);
std::string VideoCodecToString(VIDEO_CODEC codec);

std::optional<onvifXsd__VideoEncodingProfiles> EncoderProfileFromString(
        const QString& name);

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
    VIDEO_CODEC encoder = VIDEO_CODEC::NONE;
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

/*
    class VideoOptionsLocal
    {
    public:
        VideoOptionsLocal() = default;

        VideoOptionsLocal(const QString& id, const VideoOptionsResp& resp,
            QnBounds frameRateBounds = QnBounds());

        QVector<onvifXsd__H264Profile> h264Profiles;
        QString id;
        QList<QSize> resolutions;
        bool isH264 = false;
        int minQ = -1;
        int maxQ = 1;
        int frameRateMin = -1;
        int frameRateMax = -1;
        int govMin = -1;
        int govMax = -1;
        bool usedInProfiles = false;
        QString currentProfile;

    private:
        int restrictFrameRate(int frameRate, QnBounds frameRateBounds) const;
    };
*/
