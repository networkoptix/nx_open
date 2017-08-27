#pragma once

#include <QtCore/QSize>

#include <boost/optional/optional.hpp>

#include <common/common_globals.h>

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

    boost::optional<int> govLength = -1;
    boost::optional<int> maxGovLength = -1;
    boost::optional<int> minGovLength = -1;

    boost::optional<bool> dynamicGovEnabled = false;
    boost::optional<int> dynamicGovLength = -1;
    boost::optional<int> maxDynamicGovLength = -1;

    /*boost::optional<H264Profile> h264Profile;
    boost::optional<HevcProfile> hevcProfile;*/
    boost::optional<Qn::EntropyCoding> entropyCoding;
    
    bool audioEnabled = false;
    bool fixed = false;
    QString token;

    void setParameter(const QString& parameterName, const QString& parameterValue);
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
