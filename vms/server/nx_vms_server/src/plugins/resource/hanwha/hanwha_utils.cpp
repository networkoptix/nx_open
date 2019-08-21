#include "hanwha_utils.h"
#include "hanwha_common.h"

#include <utils/common/app_info.h>

namespace nx {
namespace vms::server {
namespace plugins {

QStringList fromHanwhaInternalRange(const QStringList& internalRange)
{
    return internalRange; //< TODO: #dmishin implement
}

QString channelParameter(int channelNumber, const QString& parameterName)
{
    return lit("%1.%2.%3")
        .arg(kHanwhaChannelProperty)
        .arg(channelNumber)
        .arg(parameterName);
}

boost::optional<bool> toBool(const boost::optional<QString>& str)
{
    if (!str.is_initialized())
        return boost::none;

    auto lowerCase = str->toLower();
    if (lowerCase == kHanwhaTrue.toLower())
        return true;
    else if (lowerCase == kHanwhaFalse.toLower())
        return false;

    return boost::none;
}

boost::optional<int> toInt(const boost::optional<QString>& str)
{
    if (!str.is_initialized())
        return boost::none;

    bool success = false;
    int numericValue = str->toInt(&success);

    if (!success)
        return boost::none;

    return numericValue;
}

boost::optional<double> toDouble(const boost::optional<QString>& str)
{
    if (!str.is_initialized())
        return boost::none;

    bool success = false;
    int numericValue = str->toDouble(&success);

    if (!success)
        return boost::none;

    return numericValue;
}

boost::optional<AVCodecID> toCodecId(const boost::optional<QString>& str)
{
    if (!str.is_initialized())
        return boost::none;

    return fromHanwhaString<AVCodecID>(*str);
}

boost::optional<QSize> toQSize(const boost::optional<QString>& str)
{
    if (!str.is_initialized())
        return boost::none;

    return fromHanwhaString<QSize>(*str);
}

HanwhaChannelProfiles parseProfiles(
    const HanwhaResponse& response,
    const boost::optional<int>& forcedChannel)
{
    NX_ASSERT(response.isSuccessful());
    if (!response.isSuccessful())
        return HanwhaChannelProfiles();

    HanwhaChannelProfiles profiles;
    for (const auto& entry: response.response())
    {
        const auto split = entry.first.split(L'.');
        const auto splitSize = split.size();

        if (splitSize < 5)
            continue;

        if (split[0] != kHanwhaChannelProperty)
            continue;

        bool success = false;
        auto profileChannel = kHanwhaInvalidChannel;
        if (forcedChannel != boost::none)
        {
            profileChannel = *forcedChannel;
        }
        else
        {
            profileChannel = split[1].toInt(&success);
            if (!success)
                continue;
        }

        if (split[2] != kHanwhaProfileNumberProperty)
            continue;

        const auto profileNumber = split[3].toInt(&success);
        if (!success)
            continue;

        profiles[profileChannel][profileNumber]
            .setParameter(split.mid(4).join(L'.'), entry.second);

        profiles[profileChannel][profileNumber].number = profileNumber;
    }

    return profiles;
}

QString profileSuffixByRole(Qn::ConnectionRole role)
{
    return role == Qn::ConnectionRole::CR_LiveVideo
        ? kHanwhaPrimaryNxProfileSuffix
        : kHanwhaSecondaryNxProfileSuffix;
}

QString profileFullProductName(const QString& applicationName)
{
    return applicationName
        .splitRef(' ')
        .last()
        .toString()
        .remove(QRegExp("[^a-zA-Z]"));
}

boost::optional<HanwhaVideoProfile> findProfile(
    const HanwhaProfileMap& profiles,
    Qn::ConnectionRole role,
    const QString& applicationName)
{
    const auto suffix = profileSuffixByRole(role);
    const auto productName = profileFullProductName(applicationName);
    boost::optional<HanwhaVideoProfile> result;

    QString bestPrefix;
    for (const auto& entry : profiles)
    {
        const auto& profile = entry.second;
        if (profile.name.endsWith(suffix))
        {
            const auto prefix = profile.name.left(profile.name.lastIndexOf(suffix));
            if (productName.startsWith(prefix) && prefix.length() > bestPrefix.length())
            {
                bestPrefix = prefix;
                result = profile;
            }
        }
    }

    return result;
};

std::set<int> findProfilesToRemove(
    const HanwhaProfileMap& profiles,
    boost::optional<HanwhaVideoProfile> primaryProfile,
    boost::optional<HanwhaVideoProfile> secondaryProfile)
{
    std::set<int> result;
    auto isTheSameProfile =
        [](const boost::optional<HanwhaVideoProfile> profile, int profileNumber)
    {
        return profile != boost::none && profileNumber == profile->number;
    };

    for (const auto& entry : profiles)
    {
        const auto profileNumber = entry.first;
        const auto& profile = entry.second;

        if (profile.isBuiltinProfile()
            || isTheSameProfile(primaryProfile, profileNumber)
            || isTheSameProfile(secondaryProfile, profileNumber))
        {
            continue;
        }

        result.insert(profileNumber);
    }

    return result;
};

boost::optional<int> extractPropertyChannel(const QString& fullPropertyName)
{
    const auto split = fullPropertyName.split(L'.', QString::SplitBehavior::SkipEmptyParts);
    if (split.size() < 2 || split[0].trimmed() != kHanwhaChannelProperty)
        return boost::none;

    bool success = false;
    const auto propertyChannel = split[1].toInt(&success);
    if (!success)
        return boost::none;

    return propertyChannel;
}

nx::core::resource::DeviceType fromHanwhaToNxDeviceType(HanwhaDeviceType hanwhaDeviceType)
{
    using namespace nx::core::resource;
    switch (hanwhaDeviceType)
    {
        case HanwhaDeviceType::nvr:
            return DeviceType::nvr;
        case HanwhaDeviceType::encoder:
            return DeviceType::encoder;
        case HanwhaDeviceType::nwc:
            return DeviceType::camera;
        default:
            return nx::core::resource::DeviceType::unknown;
    }
}

template<>
int fromHanwhaString<int>(const QString& str, bool* outSuccess)
{
    NX_ASSERT(outSuccess);
    return str.toInt(outSuccess);
}

template<>
bool fromHanwhaString(const QString& str)
{
    return str.toLower() == kHanwhaTrue.toLower();
}

template<>
AVCodecID fromHanwhaString(const QString& str)
{
    if (str == kHanwhaMjpeg)
        return AVCodecID::AV_CODEC_ID_MJPEG;
    else if (str == kHanwhaH264)
        return AVCodecID::AV_CODEC_ID_H264;
    else if (str == kHanwhaHevc)
        return AVCodecID::AV_CODEC_ID_HEVC;

    return AVCodecID::AV_CODEC_ID_NONE;
}

template<>
QSize fromHanwhaString(const QString& str)
{
    auto split = str.split(L'x');
    if (split.size() != 2)
        return QSize();

    bool success = false;
    auto width = split[0].trimmed().toInt(&success);
    if (!success)
        return QSize();

    auto height = split[1].trimmed().toInt(&success);
    if (!success)
        return QSize();

    return QSize(width, height);

}

template<>
Qn::BitrateControl fromHanwhaString(const QString& str)
{
    if (str == kHanwhaCbr)
        return Qn::BitrateControl::cbr;
    else if (str == kHanwhaVbr)
        return Qn::BitrateControl::vbr;

    return Qn::BitrateControl::undefined;
}

template<>
Qn::EncodingPriority fromHanwhaString(const QString& str)
{
    if (str == kHanwhaFrameRatePriority)
        return Qn::EncodingPriority::framerate;
    else if (str == kHanwhaCompressionLevelPriority)
        return Qn::EncodingPriority::compressionLevel;

    return Qn::EncodingPriority::undefined;
}

template<>
Qn::EntropyCoding fromHanwhaString(const QString& str)
{
    if (str == kHanwhaCabac)
        return Qn::EntropyCoding::cabac;
    else if (str == kHanwhaCavlc)
        return Qn::EntropyCoding::cavlc;

    return Qn::EntropyCoding::undefined;
}

template<>
HanwhaMediaType fromHanwhaString(const QString& str)
{
    if (str == kHanwhaLiveMediaType)
        return HanwhaMediaType::live;
    else if (str == kHanwhaSearchMediaType)
        return HanwhaMediaType::search;
    else if (str == kHanwhaBackupMediaType)
        return HanwhaMediaType::backup;

    return HanwhaMediaType::undefined;
}

template<>
HanwhaStreamingMode fromHanwhaString(const QString& str)
{
    if (str == kHanwhaIframeOnlyMode)
        return HanwhaStreamingMode::iframeOnly;
    else if (str == kHanwhaFullMode)
        return HanwhaStreamingMode::full;

    return HanwhaStreamingMode::undefined;
}

template<>
HanwhaStreamingType fromHanwhaString(const QString& str)
{
    if (str == kHanwhaRtpUnicast)
        return HanwhaStreamingType::rtpUnicast;
    else if (str == kHanwhaRtpMulticast)
        return HanwhaStreamingType::rtpMulticast;

    return HanwhaStreamingType::undefined;
}

template<>
HanwhaClientType fromHanwhaString(const QString& str)
{
    if (str == kHanwhaPcClient)
        return HanwhaClientType::pc;
    else if (str == kHanwhaMobileClient)
        return HanwhaClientType::mobile;

    return HanwhaClientType::undefined;
}

QString toHanwhaString(bool value)
{
    if (value)
        return kHanwhaTrue;

    return kHanwhaFalse;
}

QString toHanwhaString(AVCodecID codecId)
{
    switch (codecId)
    {
        case AVCodecID::AV_CODEC_ID_H264:
            return kHanwhaH264;
        case AVCodecID::AV_CODEC_ID_HEVC:
            return kHanwhaHevc;
        case AVCodecID::AV_CODEC_ID_MJPEG:
            return kHanwhaMjpeg;
        default:
            return kHanwhaH264; //< Is it right ???
    }
}

QString toHanwhaString(const QSize& value)
{
    return lit("%1x%2")
        .arg(value.width())
        .arg(value.height());
}


QString toHanwhaString(Qn::BitrateControl bitrateControl)
{
    switch (bitrateControl)
    {
        case Qn::BitrateControl::cbr:
            return kHanwhaCbr;
        case Qn::BitrateControl::vbr:
            return kHanwhaVbr;
        default:
            return kHanwhaVbr;
    }
}

QString toHanwhaString(Qn::EncodingPriority encodingPriority)
{
    switch (encodingPriority)
    {
        case Qn::EncodingPriority::framerate:
            return kHanwhaFrameRatePriority;
        case Qn::EncodingPriority::compressionLevel:
            return kHanwhaCompressionLevelPriority;
        default:
            return kHanwhaFrameRatePriority;
    }
}

QString toHanwhaString(Qn::EntropyCoding entropyCoding)
{
    switch (entropyCoding)
    {
        case Qn::EntropyCoding::cabac:
            return kHanwhaCabac;
        case Qn::EntropyCoding::cavlc:
            return kHanwhaCavlc;
        default:
            return kHanwhaCabac;
    }
}

QString toHanwhaString(HanwhaMediaType mediaType)
{
    switch (mediaType)
    {
        case HanwhaMediaType::live:
            return kHanwhaLiveMediaType;
        case HanwhaMediaType::search:
            return kHanwhaSearchMediaType;
        case HanwhaMediaType::backup:
            return kHanwhaBackupMediaType;
        default:
            return kHanwhaLiveMediaType;
    }
}

QString toHanwhaString(HanwhaStreamingMode streamingMode)
{
    switch (streamingMode)
    {
        case HanwhaStreamingMode::iframeOnly:
            return kHanwhaIframeOnlyMode;
        case HanwhaStreamingMode::full:
            return kHanwhaFullMode;
        default:
            return kHanwhaFullMode;
    }
}

QString toHanwhaString(HanwhaStreamingType streamingType)
{
    switch (streamingType)
    {
        case HanwhaStreamingType::rtpUnicast:
            return kHanwhaRtpUnicast;
        case HanwhaStreamingType::rtpMulticast:
            return kHanwhaRtpMulticast;
        default:
            return kHanwhaRtpUnicast;
    }
}

QString toHanwhaString(HanwhaClientType clientType)
{
    switch (clientType)
    {
        case HanwhaClientType::pc:
            return kHanwhaPcClient;
        case HanwhaClientType::mobile:
            return kHanwhaMobileClient;
        default:
            return kHanwhaPcClient;
    }
}

bool areaComparator(const QString& lhs, const QString& rhs)
{
    const auto firstWidthAndHeight = lhs.toLower().split(L'x');
    const auto secondWidthAndHeight = rhs.toLower().split(L'x');

    bool firstIsValid = firstWidthAndHeight.size() == 2;
    bool secondIsValid = secondWidthAndHeight.size() == 2;

    if (!firstIsValid || !secondIsValid)
    {
        if (!firstIsValid && !secondIsValid)
            return lhs > rhs;

        return firstIsValid > secondIsValid;
    }

    bool success = false;
    int width1 = firstWidthAndHeight[0].toInt(&success);
    if (!success)
        firstIsValid = false;

    int height1 = firstWidthAndHeight[1].toInt(&success);
    if (!success)
        firstIsValid = false;

    int width2 = secondWidthAndHeight[0].toInt(&success);
    if (!success)
        secondIsValid = false;

    int height2 = secondWidthAndHeight[1].toInt(&success);
    if (!success)
        secondIsValid = false;

    if (!firstIsValid || !secondIsValid)
    {
        if (!firstIsValid && !secondIsValid)
            return lhs > rhs;

        return firstIsValid > secondIsValid;
    }

    return width1 * height1 > width2 * height2;
}

bool ratioComparator(const QString& lhs, const QString& rhs)
{
    auto numDen1 = lhs.split(L'/');
    auto numDen2 = rhs.split(L'/');

    if (numDen1.size() == 1)
        numDen1.push_back(lit("1"));

    if (numDen2.size() == 1)
        numDen2.push_back(lit("1"));

    bool firstIsValid = numDen1.size() == 2;
    bool secondIsValid = numDen2.size() == 2;

    if (!firstIsValid || !secondIsValid)
    {
        if (!firstIsValid && !secondIsValid)
            return lhs > rhs;

        return firstIsValid > secondIsValid;
    }

    bool success = false;
    int num1 = numDen1[0].toInt(&success);
    if (!success)
        firstIsValid = false;

    int den1 = numDen1[1].toInt(&success);
    if (!success || den1 == 0)
        firstIsValid = false;

    int num2 = numDen2[0].toInt(&success);
    if (!success)
        secondIsValid = false;

    int den2 = numDen2[1].toInt(&success);
    if (!success || den2 == 0)
        secondIsValid = false;

    if (!firstIsValid || !secondIsValid)
    {
        if (!firstIsValid && !secondIsValid)
            return lhs > rhs;

        return firstIsValid > secondIsValid;
    }

    return ((double) num1 / den1) > ((double) num2 / den2);
}

qint64 hanwhaDateTimeToMsec(const QByteArray& value, std::chrono::milliseconds timeShift)
{
    // A date and time string can have a "DST" suffix (e.g "2019-06-02T00:26:37Z DST").
    // We need to get rid of it to properly parse a date.
    const auto split = value.trimmed().split(' ');
    if (split.isEmpty())
        return AV_NOPTS_VALUE;

    const auto& valueWithoutDst = split[0].trimmed();
    const auto dateTime =
        QDateTime::fromString(valueWithoutDst, Qt::ISODate)
            .addMSecs(std::chrono::milliseconds(timeShift).count());

    return std::max(0LL, dateTime.toMSecsSinceEpoch());
}

QDateTime toHanwhaDateTime(qint64 valueMs, std::chrono::milliseconds timeShift)
{
    return QDateTime::fromMSecsSinceEpoch(valueMs, Qt::OffsetFromUTC)
        .addMSecs(-std::chrono::milliseconds(timeShift).count());
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
