#include "video_encoder_config_options.h"

namespace
{

static const std::vector<std::pair<std::string, SupportedVideoCodecFlavor>>
    kSupportedVideoCodecFlavorOnvifMap = {
        { "JPEG", SupportedVideoCodecFlavor::JPEG },
        { "H264", SupportedVideoCodecFlavor::H264 },
        { "H265", SupportedVideoCodecFlavor::H265 }
    };

static const std::vector<std::pair<std::string, SupportedVideoCodecFlavor>>
    kSupportedVideoCodecFlavorVmsMap = {
        { "MJPEG", SupportedVideoCodecFlavor::JPEG },
        { "H264", SupportedVideoCodecFlavor::H264 },
        { "H265", SupportedVideoCodecFlavor::H265 }
    };

static const std::vector<std::pair<std::string, onvifXsd__H264Profile>>
    kH264ProfileOnvifMap = {
        { "Baseline", onvifXsd__H264Profile::Baseline },
        { "Main", onvifXsd__H264Profile::Main },
        { "Extended", onvifXsd__H264Profile::Extended },
        { "High", onvifXsd__H264Profile::High }
    };

static const std::vector<std::pair<std::string, onvifXsd__VideoEncodingProfiles>>
    kVideoEncoderProfilesOnvifMap = {
        { "Simple", onvifXsd__VideoEncodingProfiles::Simple }, //< MPEG4 SP
        { "AdvancedSimple", onvifXsd__VideoEncodingProfiles::AdvancedSimple }, //< MPEG4 ASP
        { "Baseline", onvifXsd__VideoEncodingProfiles::Baseline }, //< H264 Baseline
        { "Main", onvifXsd__VideoEncodingProfiles::Main }, //< H264 Main, H265 Main
        { "Main10", onvifXsd__VideoEncodingProfiles::Main10 }, //< H265 Main 10
        { "Extended", onvifXsd__VideoEncodingProfiles::Extended }, //< H264 Extended
        { "High", onvifXsd__VideoEncodingProfiles::High } //< H264 High
    };

} //namespace

SupportedVideoCodecFlavor supportedVideoCodecFlavorFromOnvifString(const std::string& name)
{
    for (const auto& pair: kSupportedVideoCodecFlavorOnvifMap)
    {
        if (pair.first == name)
            return pair.second;
    }
    return SupportedVideoCodecFlavor::NONE;
}

std::string supportedVideoCodecFlavorToOnvifString(SupportedVideoCodecFlavor codec)
{
    for (const auto& pair: kSupportedVideoCodecFlavorOnvifMap)
    {
        if (pair.second == codec)
            return pair.first;
    }
    return std::string();
}

SupportedVideoCodecFlavor supportedVideoCodecFlavorFromVmsString(const std::string& name)
{
    for (const auto& pair: kSupportedVideoCodecFlavorVmsMap)
    {
        if (pair.first == name)
            return pair.second;
    }
    return SupportedVideoCodecFlavor::NONE;
}

std::string supportedVideoCodecFlavorToVmsString(SupportedVideoCodecFlavor codec)
{
    for (const auto& pair: kSupportedVideoCodecFlavorVmsMap)
    {
        if (pair.second == codec)
            return pair.first;
    }
    return std::string();
}

std::optional<onvifXsd__H264Profile> h264ProfileFromOnvifString(const std::string& name)
{
    for (const auto& pair : kH264ProfileOnvifMap)
    {
        if (pair.first == name)
            return std::make_optional(pair.second);
    }
    return std::nullopt;
}

std::string h264ProfileToOnvifString(onvifXsd__H264Profile profile)
{
    for (const auto& pair : kH264ProfileOnvifMap)
    {
        if (pair.second == profile)
            return pair.first;
    }
    NX_ASSERT(0); //< unexpected onvifXsd__VideoEncodingProfiles value
    return std::string();
}

std::optional<onvifXsd__VideoEncodingProfiles> videoEncodingProfilesFromOnvifString(const std::string& name)
{
    for (const auto& pair: kVideoEncoderProfilesOnvifMap)
    {
        if (pair.first == name)
            return std::make_optional(pair.second);
    }
    return std::nullopt;
}

std::string VideoEncodingProfilesToString(onvifXsd__VideoEncodingProfiles profile)
{
    for (const auto& pair : kVideoEncoderProfilesOnvifMap)
    {
        if (pair.second == profile)
            return pair.first;
    }
    NX_ASSERT(0); //< unexpected onvifXsd__VideoEncodingProfiles value
    return std::string();
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const onvifXsd__JpegOptions& options)
{
    mediaWebserviseVersion = 1;
    encoder = SupportedVideoCodecFlavor::JPEG;
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
    NX_ASSERT(0); //< Mgeg4 is not supported.
    mediaWebserviseVersion = 1;
    //encoder = SupportedVideoCodecFlavor::MPEG4;
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
    encoder = SupportedVideoCodecFlavor::H264;
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

    encoder = supportedVideoCodecFlavorFromOnvifString(options.Encoding);

    if (options.QualityRange)
        qualityRange = Range<float>(options.QualityRange->Min, options.QualityRange->Max);

    if (options.BitrateRange)
        bitrateRange = Range<int>(options.BitrateRange->Min, options.BitrateRange->Max);

    if (options.ConstantBitRateSupported)
        constantBitrateSupported = *options.ConstantBitRateSupported;

    if (options.GovLengthRange)
    {
        QString govString = QString::fromLatin1(options.GovLengthRange->c_str());
        QStringList govStringList = govString.split(" ", QString::SkipEmptyParts);
        if (govStringList.size() >= 2)
        {
            govLengthRange =
                Range<int>(govStringList.front().toInt(), govStringList.back().toInt());
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
        std::sort(frameRates.begin(), frameRates.end());
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
                videoEncodingProfilesFromOnvifString(item.toStdString());
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
