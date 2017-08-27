#include "hanwha_video_profile.h"
#include "hanwha_utils.h"

#define NX_HANWHA_FROM_STRING_TO_INT(integerTarget, stringParameter)\
{\
    bool success = false;\
    auto tmp = (stringParameter).toInt(&success);\
    if (success)\
        (integerTarget) = tmp;\
}  

namespace nx {
namespace mediaserver_core {
namespace plugins {

void HanwhaVideoProfile::setParameter(
    const QString& parameterName,
    const QString& parameterValue)
{
    if (parameterName == lit("Name"))
        name = parameterValue;
    else if (parameterName == lit("EncodingType"))
        codec = fromHanwhaString<AVCodecID>(parameterValue);
    else if (parameterName == lit("Resolution"))
        resolution = fromHanwhaString<QSize>(parameterValue);
    else if (parameterName == lit("FrameRate"))
        NX_HANWHA_FROM_STRING_TO_INT(frameRate, parameterValue)
    else if (parameterName == lit("CompressionLevel"))
        NX_HANWHA_FROM_STRING_TO_INT(compressionLevel, parameterValue)
    else if (parameterName == lit("Bitrate"))
        NX_HANWHA_FROM_STRING_TO_INT(bitrateKbps, parameterValue)
    else if (parameterName.endsWith(lit("BitrateControlType")))
        bitrateControl = fromHanwhaString<Qn::BitrateControl>(parameterValue);
    else if (parameterName.endsWith("PriorityType"))
        encodiingPriority = fromHanwhaString<Qn::EncodingPriority>(parameterValue);
    else if (parameterName.endsWith("MaxDynamicGOVLength"))
        NX_HANWHA_FROM_STRING_TO_INT(maxDynamicGovLength, parameterValue)
    else if (parameterName.endsWith("DynamicGOVLength"))
        NX_HANWHA_FROM_STRING_TO_INT(dynamicGovLength, parameterValue)
    else if (parameterName.endsWith("MaxGOVLength"))
        NX_HANWHA_FROM_STRING_TO_INT(maxGovLength, parameterValue)
    else if (parameterName.endsWith("MinGOVLength"))
        NX_HANWHA_FROM_STRING_TO_INT(minGovLength, parameterValue)
    else if (parameterName.endsWith("GOVLength"))
        NX_HANWHA_FROM_STRING_TO_INT(govLength, parameterValue)
    else if (parameterName == lit("H264.Profile"))
        ; // TODO #dmishin implement
    else if (parameterName == lit("H265.Profile"))
        ; // TODO #dmishin implement
    else if (parameterName.endsWith("EntropyCoding"))
        entropyCoding = fromHanwhaString<Qn::EntropyCoding>(parameterValue);
    else if (parameterName.endsWith("DynamicGOVEnable"))
        dynamicGovEnabled = parameterValue == lit("True");
    else if (parameterName == lit("AudioInputEnable"))
        audioEnabled = parameterValue == lit("True");
    else if (parameterName == lit("IsFixedProfile"))
        fixed = parameterValue == lit("True");
    else if (parameterName == lit("ProfileToken"))
        token = parameterValue;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
