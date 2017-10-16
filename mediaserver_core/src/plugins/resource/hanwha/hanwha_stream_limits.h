#pragma once

#if defined(ENABLE_HANWHA)

#include <plugins/resource/hanwha/hanwha_cgi_parameter.h>

#include <vector>
#include <set>

#include <utils/media/h264_common.h>
#include <utils/media/hevc_common.h>

#include <common/common_globals.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct HanwhaStreamLimits
{
    std::vector<QSize> resolutions;
    std::vector<int> frameRates;
    std::pair<int, int> bitrateLimits = {-1, -1};
    std::pair<int, int> govLengthLimits = {-1, -1};
    std::set<AVCodecID> codecs;
    std::set<Qn::EntropyCoding> entropyCodingTypes;
    std::set<Qn::BitrateControl> bitrateControlTypes;
    std::set<Qn::EncodingPriority> encodingPriorityTypes;
    QStringList h264Profiles;
    QStringList hevcProfiles;

    bool setLimit(const HanwhaCgiParameter& parameter);

    bool isValid() const;

private:
    template<typename Element, typename Collection>
    bool setCollectionLimits(
        const HanwhaCgiParameter& parameter,
        Collection* collection);

    bool setCollectionLimits(
        const HanwhaCgiParameter& parameter,
        std::vector<int>* collection);

    bool setIntegerLimits(
        const HanwhaCgiParameter& parameter,
        std::pair<int, int>* integerLimits);
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
