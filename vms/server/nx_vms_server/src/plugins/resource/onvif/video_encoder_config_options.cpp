#include "video_encoder_config_options.h"

namespace
{

static const std::vector<std::pair<std::string, SupportedVideoEncoding>>
    kSupportedVideoCodecFlavorOnvifMap = {
        { "JPEG", SupportedVideoEncoding::JPEG },
        { "H264", SupportedVideoEncoding::H264 },
        { "H265", SupportedVideoEncoding::H265 }
    };

static const std::vector<std::pair<std::string, SupportedVideoEncoding>>
    kSupportedVideoCodecFlavorVmsMap = {
        { "MJPEG", SupportedVideoEncoding::JPEG },
        { "H264", SupportedVideoEncoding::H264 },
        { "H265", SupportedVideoEncoding::H265 }
    };

static const std::vector<std::pair<std::string, tt__H264Profile>>
    kH264ProfileOnvifMap = {
        { "Baseline", tt__H264Profile::Baseline },
        { "Main", tt__H264Profile::Main },
        { "Extended", tt__H264Profile::Extended },
        { "High", tt__H264Profile::High }
    };

static const std::vector<std::pair<std::string, tt__VideoEncodingProfiles>>
    kVideoEncoderProfilesOnvifMap = {
        { "Simple", tt__VideoEncodingProfiles::Simple }, //< MPEG4 SP
        { "AdvancedSimple", tt__VideoEncodingProfiles::AdvancedSimple }, //< MPEG4 ASP
        { "Baseline", tt__VideoEncodingProfiles::Baseline }, //< H264 Baseline
        { "Main", tt__VideoEncodingProfiles::Main }, //< H264 Main, H265 Main
        { "Main10", tt__VideoEncodingProfiles::Main10 }, //< H265 Main 10
        { "Extended", tt__VideoEncodingProfiles::Extended }, //< H264 Extended
        { "High", tt__VideoEncodingProfiles::High } //< H264 High
    };

} //namespace

SupportedVideoEncoding supportedVideoCodecFlavorFromOnvifString(const std::string& name)
{
    for (const auto& pair: kSupportedVideoCodecFlavorOnvifMap)
    {
        if (pair.first == name)
            return pair.second;
    }
    return SupportedVideoEncoding::NONE;
}

std::string supportedVideoCodecFlavorToOnvifString(SupportedVideoEncoding codec)
{
    for (const auto& pair: kSupportedVideoCodecFlavorOnvifMap)
    {
        if (pair.second == codec)
            return pair.first;
    }
    return std::string();
}

SupportedVideoEncoding supportedVideoCodecFlavorFromVmsString(const std::string& name)
{
    for (const auto& pair: kSupportedVideoCodecFlavorVmsMap)
    {
        if (pair.first == name)
            return pair.second;
    }
    return SupportedVideoEncoding::NONE;
}

std::string supportedVideoCodecFlavorToVmsString(SupportedVideoEncoding codec)
{
    for (const auto& pair: kSupportedVideoCodecFlavorVmsMap)
    {
        if (pair.second == codec)
            return pair.first;
    }
    return std::string();
}

std::optional<tt__H264Profile> h264ProfileFromOnvifString(const std::string& name)
{
    for (const auto& pair : kH264ProfileOnvifMap)
    {
        if (pair.first == name)
            return std::make_optional(pair.second);
    }
    return std::nullopt;
}

std::string h264ProfileToOnvifString(tt__H264Profile profile)
{
    for (const auto& pair : kH264ProfileOnvifMap)
    {
        if (pair.second == profile)
            return pair.first;
    }
    NX_ASSERT(0); //< unexpected tt__VideoEncodingProfiles value
    return std::string();
}

std::optional<tt__VideoEncodingProfiles> videoEncodingProfilesFromOnvifString(const std::string& name)
{
    for (const auto& pair: kVideoEncoderProfilesOnvifMap)
    {
        if (pair.first == name)
            return std::make_optional(pair.second);
    }
    return std::nullopt;
}

std::string videoEncodingProfilesToString(tt__VideoEncodingProfiles profile)
{
    for (const auto& pair : kVideoEncoderProfilesOnvifMap)
    {
        if (pair.second == profile)
            return pair.first;
    }
    NX_ASSERT(0); //< unexpected tt__VideoEncodingProfiles value
    return std::string();
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const tt__JpegOptions& options)
{
    mediaWebserviseVersion = 1;
    encoder = SupportedVideoEncoding::JPEG;
    // qualityRange: tt__JpegOptions contains no such information
    // bitrateRange: tt__JpegOptions contains no such information
    // ConstantBitRateSupported: tt__JpegOptions contains no such information
    // govLengthRange: : tt__JpegOptions contains no such information
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
    // tt__JpegOptions::EncodingIntervalRange is ignored
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const tt__Mpeg4Options& options)
{
    NX_ASSERT(0); //< Mgeg4 is not supported.
    mediaWebserviseVersion = 1;
    //encoder = SupportedVideoEncoding::MPEG4;
    // qualityRange: tt__Mpeg4Options contains no such information
    // bitrateRange: tt__Mpeg4Options contains no such information
    // ConstantBitRateSupported: tt__Mpeg4Options contains no such information

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

    for (tt__Mpeg4Profile profile: options.Mpeg4ProfilesSupported)
    {
        switch (profile)
        {
            case tt__Mpeg4Profile::SP:
                encoderProfiles.push_back(tt__VideoEncodingProfiles::Simple);
                break;
            case tt__Mpeg4Profile::ASP:
                encoderProfiles.push_back(tt__VideoEncodingProfiles::AdvancedSimple);
                break;
        }
    }

    // tt__Mpeg4Options::EncodingIntervalRange is ignored
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(const tt__H264Options& options)
{
    mediaWebserviseVersion = 1;
    encoder = SupportedVideoEncoding::H264;
    // qualityRange: tt__Mpeg4Options contains no such information
    // bitrateRange: tt__Mpeg4Options contains no such information
    // ConstantBitRateSupported: tt__Mpeg4Options contains no such information

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

    for (tt__H264Profile profile : options.H264ProfilesSupported)
    {
        switch (profile)
        {
        case tt__H264Profile::Baseline:
            encoderProfiles.push_back(tt__VideoEncodingProfiles::Baseline);
            break;
        case tt__H264Profile::Main:
            encoderProfiles.push_back(tt__VideoEncodingProfiles::Main);
            break;
        case tt__H264Profile::Extended:
            encoderProfiles.push_back(tt__VideoEncodingProfiles::Extended);
            break;
        case tt__H264Profile::High:
            encoderProfiles.push_back(tt__VideoEncodingProfiles::High);
            break;
        }
    }
}

VideoEncoderConfigOptions::VideoEncoderConfigOptions(
    const tt__VideoEncoder2ConfigurationOptions& options)
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
            std::optional<tt__VideoEncodingProfiles> profile =
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
