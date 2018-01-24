#include "hanwha_codec_limits.h"

#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

template<typename T>
bool setValue(T* target, const T& value)
{
    *target = value;
    return true;
}

} // namespace

bool HanwhaCodecLimits::setLimit(const QString& limitName, const QString& limitValue)
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
        return setValue(&maxFps, value / 1000);

    if (limitName == kHanwhaDefaultFpsProperty)
        return setValue(&defaultFps, value / 1000);

    return false;
}

HanwhaCodecInfo::HanwhaCodecInfo(const HanwhaResponse& response)
{
    m_isValid = parseResponse(response);
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
        return boost::none;

    auto codecInfo = channelInfo->second.find(codec);
    if (codecInfo == channelInfo->second.cend())
        return boost::none;

    auto streamTypeInfo = codecInfo->second.find(streamType);
    if (streamTypeInfo == codecInfo->second.cend())
        return boost::none;

    auto resolutionInfo = streamTypeInfo->second.find(resolution);
    if (resolutionInfo == streamTypeInfo->second.cend())
        return boost::none;

    return resolutionInfo->second;
}

boost::optional<HanwhaCodecLimits> HanwhaCodecInfo::limits(int channel, const QString& path) const
{
    auto split = path.split(L'/');
    if (split.size() != 3)
        return boost::none;

    return limits(channel, split[0], split[1], split[2]);
}

std::set<QSize> HanwhaCodecInfo::resolutions(
    int channel,
    AVCodecID codec,
    const QString& streamType) const
{
    const auto codecString = toHanwhaString(codec);
    return resolutions(channel, codecString, streamType);
}

std::set<QSize> HanwhaCodecInfo::resolutions(
    int channel,
    const QString& codec,
    const QString& streamType) const
{
    const QString channelString = QString::number(channel);
    std::set<QSize> result;

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
        result.insert(fromHanwhaString<QSize>(entry.first));

    return result;
}

std::set<QSize> HanwhaCodecInfo::resolutions(int channel, const QString& path) const
{
    const auto split = path.split(L'/');
    if (split.size() != 2)
        return std::set<QSize>();

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

bool HanwhaCodecInfo::isValid() const
{
    return m_isValid;
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

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
