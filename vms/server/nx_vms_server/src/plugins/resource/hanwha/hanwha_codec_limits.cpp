#include "hanwha_codec_limits.h"

#include "hanwha_utils.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

template<typename T>
bool setValue(T* target, const T& value)
{
    *target = value;
    return true;
}

static const std::map<QString, AVCodecID> kCodecProfileParameters = {
    {lit("H264.Profile"), AVCodecID::AV_CODEC_ID_H264},
    {lit("H265.Profile"), AVCodecID::AV_CODEC_ID_H265}
};

} // namespace

bool HanwhaCodecLimits::setLimit(
    const QString& limitName,
    const QString& limitValue)
{
    bool success = false;
    int value = limitValue.toInt(&success);
    if (!success)
        return false;

    if (limitName == kHanwhaMinVbrBitrateProperty)
        return setValue(&minVbrBitrate, value);

    if (limitName == kHanwhaMaxVbrBitrateProperty)
        return setValue(&maxVbrBitrate, value);

    if (limitName == kHanwhaDefaultVbrBitrateProperty)
        return setValue(&defaultVbrBitrate, value);

    if (limitName == kHanwhaMinCbrBitrateProperty)
        return setValue(&minCbrBitrate, value);

    if (limitName == kHanwhaMaxCbrBitrateProperty)
        return setValue(&maxCbrBitrate, value);

    if (limitName == kHanwhaDefaultCbrBitrateProperty)
        return setValue(&defaultCbrBitrate, value);

    if (limitName == kHanwhaMaxFpsProperty)
        return setValue(&maxFps, value / (value < 1000 ? 1 : 1000));

    if (limitName == kHanwhaDefaultFpsProperty)
        return setValue(&defaultFps, value / (value < 1000 ? 1 : 1000));

    return false;
}

HanwhaCodecInfo::HanwhaCodecInfo(
    const HanwhaResponse& response,
    const HanwhaCgiParameters& cgiParameters)
{
    m_isValid = parseResponse(response);
    if (m_isValid)
        m_isValid &= fetchCodecProfiles(cgiParameters);
}

boost::optional<HanwhaCodecLimits> HanwhaCodecInfo::limits(
    int channel,
    AVCodecID codec,
    const QString& streamType,
    const QSize& resolution) const
{
    const auto codecString = toHanwhaString(codec);
    const auto resolutionString = toHanwhaString(resolution);

    return limits(channel, codecString, streamType, resolutionString);
}

boost::optional<HanwhaCodecLimits> HanwhaCodecInfo::limits(
    int channel,
    const QString& codec,
    const QString& streamType,
    const QString& resolution) const
{
    const auto channelString = QString::number(channel);

    auto channelInfo = m_channelInfo.find(channelString);
    if (channelInfo == m_channelInfo.cend())
    {
        NX_WARNING(this, lm("Channel information for channel %1 not found in cache").args(channel));
        return boost::none;
    }

    auto codecInfo = channelInfo->second.find(codec);
    if (codecInfo == channelInfo->second.cend())
    {
        NX_WARNING(this, lm("Codec information for codec %1 not found in cache").args(codec));
        return boost::none;
    }

    auto streamTypeInfo = codecInfo->second.find(streamType);
    if (streamTypeInfo == codecInfo->second.cend())
        return boost::none;

    auto resolutionInfo = streamTypeInfo->second.find(resolution);
    if (resolutionInfo == streamTypeInfo->second.cend())
    {
        NX_WARNING(
            this,
            lm("Resolution information for resolution %1 not found in cache").args(resolution));
        return boost::none;
    }

    return resolutionInfo->second;
}

boost::optional<HanwhaCodecLimits> HanwhaCodecInfo::limits(int channel, const QString& path) const
{
    auto split = path.split(L'/');
    if (split.size() != 3)
        return boost::none;

    return limits(channel, split[0], split[1], split[2]);
}

std::vector<QSize> HanwhaCodecInfo::resolutions(
    int channel,
    AVCodecID codec,
    const QString& streamType) const
{
    const auto codecString = toHanwhaString(codec);
    return resolutions(channel, codecString, streamType);
}

std::vector<QSize> HanwhaCodecInfo::resolutions(
    int channel,
    const QString& codec,
    const QString& streamType) const
{
    const QString channelString = QString::number(channel);
    std::vector<QSize> result;

    auto channelInfo = m_channelInfo.find(channelString);
    if (channelInfo == m_channelInfo.cend())
        return result;

    auto codecInfo = channelInfo->second.find(codec);
    if (codecInfo == channelInfo->second.cend())
        return result;

    auto streamTypeInfo = codecInfo->second.find(streamType);
    if (streamTypeInfo == codecInfo->second.cend())
        return result;

    for (const auto& entry: streamTypeInfo->second)
        result.push_back(fromHanwhaString<QSize>(entry.first));

    sortResolutions(&result);
    return result;
}

std::vector<QSize> HanwhaCodecInfo::resolutions(int channel, const QString& path) const
{
    const auto split = path.split(L'/');
    if (split.size() != 2)
        return std::vector<QSize>();

    return resolutions(channel, split[0], split[1]);
}

std::set<QString> HanwhaCodecInfo::streamTypes(int channel, AVCodecID codec) const
{
    return streamTypes(channel, toHanwhaString(codec));
}

std::set<QString> HanwhaCodecInfo::streamTypes(int channel, const QString& codec) const
{
    std::set<QString> result;
    const QString channelString = QString::number(channel);

    auto channelInfo = m_channelInfo.find(channelString);
    if (channelInfo == m_channelInfo.cend())
        return result;

    auto codecInfo = channelInfo->second.find(codec);
    if (codecInfo == channelInfo->second.cend())
        return result;

    for (const auto& entry: codecInfo->second)
        result.insert(entry.first);

    return result;
}

std::set<AVCodecID> HanwhaCodecInfo::codecs(int channel) const
{
    std::set<AVCodecID> result;
    auto channelString = QString::number(channel);
    auto channelInfo = m_channelInfo.find(channelString);

    if (channelInfo == m_channelInfo.cend())
        return result;

    for (const auto& entry: channelInfo->second)
        result.insert(fromHanwhaString<AVCodecID>(entry.first));

    return result;
}

QStringList HanwhaCodecInfo::codecProfiles(AVCodecID codec) const
{
    const auto profiles = m_codecProfiles.find(codec);
    if (profiles == m_codecProfiles.cend())
        return QStringList();

    return profiles->second;
}

bool HanwhaCodecInfo::isValid() const
{
    return m_isValid && !m_channelInfo.empty();
}

void HanwhaCodecInfo::updateToChannel(int channel)
{
    NX_ASSERT(!m_channelInfo.empty());
    if (m_channelInfo.empty())
        return;

    ChannelInfo channelInfo;
    channelInfo.emplace(QString::number(channel), std::move(m_channelInfo.begin()->second));
    m_channelInfo = std::move(channelInfo);
}

bool HanwhaCodecInfo::parseResponse(const HanwhaResponse& response)
{
    if (!response.isSuccessful())
        return false;

    const auto parameters = response.response();

    for (const auto& entry: parameters)
    {
        const auto& parameterName = entry.first;
        const auto& parameterValue = entry.second;

        const auto split = parameterName.split(L'.');

        if (split.size() != 6)
            continue;

        if (split[0] != kHanwhaChannelProperty)
            continue;

        auto& limits = m_channelInfo
            [split[1] /*Channel*/]
            [split[2] /*Codec*/]
            [split[3] /*StreamType*/]
            [split[4].toLower() /*Resolution*/];

        limits.setLimit(split[5], parameterValue);
    }

    return true;
}

void HanwhaCodecInfo::sortResolutions(std::vector<QSize>* resolutions)
{
    std::sort(
        resolutions->begin(),
        resolutions->end(),
        [](const QSize& f, const QSize& s)
        {
            return f.width() * f.height() > s.width() * s.height();
        });
}

bool HanwhaCodecInfo::fetchCodecProfiles(const HanwhaCgiParameters& cgiParameters)
{
    for (const auto& item: kCodecProfileParameters)
    {
        const auto& profileParameter = item.first;
        auto codecId = item.second;

        const auto profiles = cgiParameters.parameter(
            lit("media/videoprofile/add_update/%1").arg(profileParameter));

        if (profiles != boost::none)
            m_codecProfiles[codecId] = profiles->possibleValues();
    }

    return true;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
