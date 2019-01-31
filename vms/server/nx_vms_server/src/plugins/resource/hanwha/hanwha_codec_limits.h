#pragma once

#include <set>

#include <QtCore/QSize>

#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace vms::server {
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
    HanwhaCodecInfo(
        const HanwhaResponse& response,
        const HanwhaCgiParameters& cgiParameters);

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

    // @return sorted list of resolutions.
    std::vector<QSize> resolutions(int channel, AVCodecID codec, const QString& streamType) const;

    // @return sorted list of resolutions.
    std::vector<QSize> resolutions(
        int channel,
        const QString& codec,
        const QString& streamType) const;

    // @return sorted list of resolutions.
    std::vector<QSize> resolutions(int channel, const QString& path) const;

    std::set<QString> streamTypes(int channel, AVCodecID codec) const;

    std::set<QString> streamTypes(int channel, const QString& codec) const;

    std::set<AVCodecID> codecs(int channel) const;

    // TODO: #dmishin I don't know how to fetch supported codec profiles per channel for NVRs.
    QStringList codecProfiles(AVCodecID codec) const;

    bool isValid() const;

    void updateToChannel(int channel);

private:
    bool parseResponse(const HanwhaResponse& response);

    static void sortResolutions(std::vector<QSize>* resolutions);

    bool fetchCodecProfiles(const HanwhaCgiParameters& cgiParameters);

private:
    using ResolutionLimits = std::map<QString, HanwhaCodecLimits>;
    using StreamTypeLimits = std::map<QString, ResolutionLimits>;
    using CodecInfo = std::map<QString, StreamTypeLimits>;
    using ChannelInfo = std::map<QString, CodecInfo>;
    ChannelInfo m_channelInfo;

    using CodecProfiles = std::map<AVCodecID, QStringList>;
    CodecProfiles m_codecProfiles;

    bool m_isValid = false;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
