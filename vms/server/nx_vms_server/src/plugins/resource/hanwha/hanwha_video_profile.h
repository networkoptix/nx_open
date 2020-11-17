#pragma once

#include <optional>

#include <QtCore/QSize>

#include <common/common_globals.h>

#include <utils/media/h264_common.h>
#include <utils/media/hevc_common.h>

#include <plugins/resource/hanwha/hanwha_common.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace vms::server {
namespace plugins {

// TODO: #dmishin class?
struct HanwhaVideoProfile
{
    QString name;
    int number = kHanwhaInvalidProfile;
    AVCodecID codec = AVCodecID::AV_CODEC_ID_NONE;
    QSize resolution;
    int frameRate = kHanwhaInvalidFps;
    int compressionLevel = -1;
    int bitrateKbps = -1;
    bool rtpMulticastEnable = false;
    QString rtpMulticastAddress;
    int rtpMulticastPort = 0;
    int rtpMulticastTtl = 0;

    std::optional<Qn::BitrateControl> bitrateControl;
    std::optional<Qn::EncodingPriority> encodiingPriority;

    std::optional<int> govLength;
    std::optional<int> maxGovLength;
    std::optional<int> minGovLength;

    std::optional<bool> dynamicGovEnabled;
    std::optional<int> dynamicGovLength;
    std::optional<int> maxDynamicGovLength;

    std::optional<QString> codecProfile;
    std::optional<Qn::EntropyCoding> entropyCoding;

    bool audioEnabled = false;
    bool fixed = false;
    QString token;

    void setParameter(const QString& parameterName, const QString& parameterValue);

    bool isBuiltinProfile() const;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
