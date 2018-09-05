#include "video_encoder_config_options.h"

VIDEO_CODEC VideoCodecFromString(const std::string& name)
{
    VIDEO_CODEC result = VIDEO_CODEC::NONE;
    static const std::map<std::string, VIDEO_CODEC> kCodecMap = {
        { "JPEG", VIDEO_CODEC::JPEG },
        { "MPEG4", VIDEO_CODEC::MPEG4 },
        { "H264", VIDEO_CODEC::H264 },
        { "H265", VIDEO_CODEC::H265 }
    };
    const auto it = kCodecMap.find(name);
    if (it != kCodecMap.end())
        result = it->second;
    return result;
}

std::string VideoCodecToString(VIDEO_CODEC codec)
{
    std::string result;
    static const std::map<VIDEO_CODEC, std::string> kCodecMap = {
        { VIDEO_CODEC::JPEG, "JPEG" },
        { VIDEO_CODEC::MPEG4, "MPEG4" },
        { VIDEO_CODEC::H264, "H264" },
        { VIDEO_CODEC::H265, "H265" }
    };

    const auto it = kCodecMap.find(codec);
    if (it != kCodecMap.end())
        result = it->second;
    return result;
}

std::optional<onvifXsd__VideoEncodingProfiles> EncoderProfileFromString(const QString& name)
{
    std::optional<onvifXsd__VideoEncodingProfiles> result;
    static const std::map<QString, onvifXsd__VideoEncodingProfiles> kProfileMap = {
        { "Simple", onvifXsd__VideoEncodingProfiles::Simple },
        { "AdvancedSimple", onvifXsd__VideoEncodingProfiles::AdvancedSimple },
        { "Baseline", onvifXsd__VideoEncodingProfiles::Baseline },
        { "Base", onvifXsd__VideoEncodingProfiles::Baseline },
        { "Main", onvifXsd__VideoEncodingProfiles::Main },
        { "Main10", onvifXsd__VideoEncodingProfiles::Main10 },
        { "Extended", onvifXsd__VideoEncodingProfiles::Extended },
        { "High", onvifXsd__VideoEncodingProfiles::High }
    };
    const auto it = kProfileMap.find(name);
    if (it != kProfileMap.end())
        *result = it->second;
    return result;
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const onvifXsd__JpegOptions& options)
{
    mediaWebserviseVersion = 1;
    encoder = VIDEO_CODEC::JPEG;
    // qualityRange: onvifXsd__JpegOptions contains no such information
    // bitrateRange: onvifXsd__JpegOptions contains no such information
    // ConstantBitRateSupported: onvifXsd__JpegOptions contains no such information
    // govLengthRange: : onvifXsd__JpegOptions contains no such information
    for (const auto resolution : options.ResolutionsAvailable)
    {
        if (resolution)
            resolutions.push_back(QSize(resolution->Width, resolution->Height));
    }

    if (options.FrameRateRange)
    {
        frameRates.push_back(options.FrameRateRange->Min);
        frameRates.push_back(options.FrameRateRange->Max);
    }
    // onvifXsd__JpegOptions::EncodingIntervalRange is ignored
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const onvifXsd__Mpeg4Options& options)
{
    mediaWebserviseVersion = 1;
    encoder = VIDEO_CODEC::MPEG4;
    // qualityRange: onvifXsd__Mpeg4Options contains no such information
    // bitrateRange: onvifXsd__Mpeg4Options contains no such information
    // ConstantBitRateSupported: onvifXsd__Mpeg4Options contains no such information

    if (options.GovLengthRange)
        govLengthRange = Range<int>(options.GovLengthRange->Min, options.GovLengthRange->Max);

    for (const auto resolution : options.ResolutionsAvailable)
    {
        if (resolution)
            resolutions.push_back(QSize(resolution->Width, resolution->Height));
    }

    if (options.FrameRateRange)
    {
        frameRates.push_back(options.FrameRateRange->Min);
        frameRates.push_back(options.FrameRateRange->Max);
    }

    for (onvifXsd__Mpeg4Profile profile: options.Mpeg4ProfilesSupported)
    {
        switch (profile)
        {
            case onvifXsd__Mpeg4Profile::SP:
                encoderProfiles.push_back(onvifXsd__VideoEncodingProfiles::Simple);
                break;
            case onvifXsd__Mpeg4Profile::ASP:
                encoderProfiles.push_back(onvifXsd__VideoEncodingProfiles::AdvancedSimple);
                break;
        }
    }

    // onvifXsd__Mpeg4Options::EncodingIntervalRange is ignored
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const onvifXsd__H264Options& options)
{
    mediaWebserviseVersion = 1;
    encoder = VIDEO_CODEC::H264;
    // qualityRange: onvifXsd__Mpeg4Options contains no such information
    // bitrateRange: onvifXsd__Mpeg4Options contains no such information
    // ConstantBitRateSupported: onvifXsd__Mpeg4Options contains no such information

    if (options.GovLengthRange)
        govLengthRange = Range<int>(options.GovLengthRange->Min, options.GovLengthRange->Max);

    for (const auto resolution: options.ResolutionsAvailable)
    {
        if (resolution)
            resolutions.push_back(QSize(resolution->Width, resolution->Height));
    }

    if (options.FrameRateRange)
    {
        frameRates.push_back(options.FrameRateRange->Min);
        frameRates.push_back(options.FrameRateRange->Max);
    }

    for (onvifXsd__H264Profile profile : options.H264ProfilesSupported)
    {
        switch (profile)
        {
        case onvifXsd__H264Profile::Baseline:
            encoderProfiles.push_back(onvifXsd__VideoEncodingProfiles::Baseline);
            break;
        case onvifXsd__H264Profile::Main:
            encoderProfiles.push_back(onvifXsd__VideoEncodingProfiles::Main);
            break;
        case onvifXsd__H264Profile::Extended:
            encoderProfiles.push_back(onvifXsd__VideoEncodingProfiles::Extended);
            break;
        case onvifXsd__H264Profile::High:
            encoderProfiles.push_back(onvifXsd__VideoEncodingProfiles::High);
            break;
        }
    }
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(
    const onvifXsd__VideoEncoder2ConfigurationOptions& options)
{
    mediaWebserviseVersion = 2;

    encoder = VideoCodecFromString(options.Encoding);

    if (options.QualityRange)
        qualityRange = Range<float>(options.QualityRange->Min, options.QualityRange->Max);

    if (options.BitrateRange)
        bitrateRange = Range<int>(options.BitrateRange->Min, options.BitrateRange->Max);

    if (options.ConstantBitRateSupported)
        *constantBitrateSupported = *options.ConstantBitRateSupported;

    if (options.GovLengthRange)
    {
        QString govString = QString::fromLatin1(options.GovLengthRange->c_str());
        QStringList govStringList = govString.split(" ", QString::SkipEmptyParts);
        if (govStringList.size() >= 2)
        {
            govLengthRange =
                Range(govStringList.front().toInt(), govStringList.back().toInt());
        }
    }

    resolutions.reserve(options.ResolutionsAvailable.size());
    for (const auto resolution: options.ResolutionsAvailable)
    {
        if (resolution)
            resolutions.push_back(QSize(resolution->Width, resolution->Height));
    }

    if (options.FrameRatesSupported)
    {
        QString frameRatesString = QString::fromLatin1(options.FrameRatesSupported->c_str());
        QStringList frameRatesStringList =
            frameRatesString.split(" ", QString::SkipEmptyParts);
        frameRates.reserve(frameRatesStringList.size());
        for (const auto& item : frameRatesStringList)
        {
            bool ok = false;
            int frameRateInt = item.toInt(&ok);
            if (ok)
                frameRates.push_back(frameRateInt);
        }
    }

    if (options.ProfilesSupported)
    {
        QString profilesString = QString::fromLatin1(options.ProfilesSupported->c_str());
        QStringList ProfilesStringList =
            profilesString.split(" ", QString::SkipEmptyParts);
        for (const auto& item : ProfilesStringList)
        {
            bool ok = false;
            std::optional<onvifXsd__VideoEncodingProfiles> profile =
                EncoderProfileFromString(item);
            if (profile)
                encoderProfiles.push_back(*profile);
        }
    }

}

/* Process data received through Media1 web service*/
VideoEncoderConfigOptionsList::VideoEncoderConfigOptionsList(
    const _onvifMedia__GetVideoEncoderConfigurationOptionsResponse& response)
{
    if (!response.Options)
        return;

    Range<float> qualityRange;
    if (response.Options->QualityRange)
    {
        qualityRange =
            Range<float>(response.Options->QualityRange->Min, response.Options->QualityRange->Max);
    }

    if (response.Options->JPEG)
        emplace_back(*response.Options->JPEG);

    /* We don't support MPEG4 */
#if 0
    if (response.Options->MPEG4)
        emplace_back(*response.Options->MPEG4);
#endif

    if (response.Options->H264)
        emplace_back(*response.Options->H264);

    /* We don't use extended codecs */
#if 0
    if (response.Options->Extension)
    {
        if (response.Options->Extension->JPEG)
            emplace_back(*response.Options->Extension->JPEG);

        if (response.Options->Extension->MPEG4)
            emplace_back(*response.Options->Extension->MPEG4);

        if (response.Options->Extension->H264)
            emplace_back(*response.Options->Extension->H264);
    }
#endif

    for (auto& item: *this)
        item.qualityRange = qualityRange;

    sort(rbegin(), rend()); //< sort by encoder

}

/* Process data received through Media2 web service*/
VideoEncoderConfigOptionsList::VideoEncoderConfigOptionsList(
    const _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse& response)
{
    for (const auto src: response.Options)
    {
        if (src)
        {
            if (src->Encoding != "MPEG4") //< We don't support MPEG4
                emplace_back(*src);
        }
    }
    sort(rbegin(), rend()); //< sort by encoder
}
