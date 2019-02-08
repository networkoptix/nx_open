#include "rtsp_utils.h"

#include <map>
#include <vector>

namespace nx::rtsp {

AVCodecID findEncoderCodecId(const QString& codecName)
{
    QString codecLower = codecName.toLower();
    AVCodec* avCodec = avcodec_find_encoder_by_name(codecLower.toUtf8().constData());
    if (avCodec)
        return avCodec->id;

    // Try to check codec substitutes if requested codecs not found.
    static std::map<std::string, std::vector<std::string>> codecSubstitutesMap
    {
        { "h264", {"libopenh264"} },
    };
    auto substitutes = codecSubstitutesMap.find(codecLower.toUtf8().constData());
    if (substitutes == codecSubstitutesMap.end())
        return AV_CODEC_ID_NONE;

    for(auto& substitute: substitutes->second)
    {
        avCodec = avcodec_find_encoder_by_name(substitute.c_str());
        if (avCodec)
            return avCodec->id;
    }
    return AV_CODEC_ID_NONE;
}

} // namespace nx::rtsp
