#pragma once

#include <set>

#include <QtCore/QSize>

#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_response.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {



struct HanwhaCodecLimits
{
    int maxFps = kHanwhaInvalidFps;
    int defaultFps = kHanwhaInvalidFps;
    int minCbrBitrate = kHanwhaInvalidBitrate;
    int maxCbrBitrate = kHanwhaInvalidBitrate;
    int defaultCbrBitrate = kHanwhaInvalidBitrate;
    int minVbrBitrate = kHanwhaInvalidBitrate;
    int maxVbrBitrate = kHanwhaInvalidBitrate;
    int defaultVbrBitrate = kHanwhaInvalidBitrate;

    bool setLimit(const QString& limitName, const QString& limitValue);
};

class HanwhaCodecInfo
{
public:
    HanwhaCodecInfo() = default;
    explicit HanwhaCodecInfo(const HanwhaResponse& response);

    boost::optional<HanwhaCodecLimits> limits(
        int channel,
        AVCodecID codec,
        const QString& streamType,
        const QSize& resolution) const;

    boost::optional<HanwhaCodecLimits> limits(
        int channel,
        const QString& codec,
        const QString& streamType,
        const QString& resolution) const;

    boost::optional<HanwhaCodecLimits> limits(
        int channel,
        const QString& path) const;

    std::set<QSize> resolutions(int channel, AVCodecID codec, const QString& streamType) const;

    std::set<QSize> resolutions(
        int channel,
        const QString& codec,
        const QString& streamType) const;

    std::set<QSize> resolutions(int channel, const QString& path) const;

    std::set<QString> streamTypes(int channel, AVCodecID codec) const;

    std::set<QString> streamTypes(int channel, const QString& codec) const;

    std::set<AVCodecID> codecs(int channel) const;

    bool isValid() const;

private:
    bool parseResponse(const HanwhaResponse& response);

private:
    using ResolutionLimits = std::map<QString, HanwhaCodecLimits>;
    using StreamTypeLimits = std::map<QString, ResolutionLimits>;
    using CodecInfo = std::map<QString, StreamTypeLimits>;
    using ChannelInfo = std::map<QString, CodecInfo>;

    ChannelInfo m_channelInfo;
    bool m_isValid = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
