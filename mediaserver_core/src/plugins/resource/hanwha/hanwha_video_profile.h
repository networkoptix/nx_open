#pragma once

#include <QtCore/QSize>

#include <boost/optional/optional.hpp>

#include <common/common_globals.h>

#include <utils/media/h264_common.h>
#include <utils/media/hevc_common.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {

// TODO: #dmishin class?
struct HanwhaVideoProfile
{
    QString name;
    int number = -1;
    AVCodecID codec = AVCodecID::AV_CODEC_ID_NONE;
    QSize resolution;
    int frameRate = -1;
    int compressionLevel = -1;
    int bitrateKbps = -1;

    boost::optional<Qn::BitrateControl> bitrateControl;
    boost::optional<Qn::EncodingPriority> encodiingPriority;

    boost::optional<int> govLength;
    boost::optional<int> maxGovLength;
    boost::optional<int> minGovLength;

    boost::optional<bool> dynamicGovEnabled;
    boost::optional<int> dynamicGovLength;
    boost::optional<int> maxDynamicGovLength;

    boost::optional<nx::media_utils::h264::Profile> h264Profile;
    boost::optional<nx::media_utils::hevc::Profile> hevcProfile;
    boost::optional<Qn::EntropyCoding> entropyCoding;
    
    bool audioEnabled = false;
    bool fixed = false;
    QString token;

    void setParameter(const QString& parameterName, const QString& parameterValue);
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
