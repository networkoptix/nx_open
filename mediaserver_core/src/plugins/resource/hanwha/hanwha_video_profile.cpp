#include "hanwha_video_profile.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

template<typename T>
void setInteger(T* target, const QString& stringParameter)
{
    bool success = false;
    auto tmp = stringParameter.toInt(&success);
    if (success)
        *target = tmp;
}

const QSet<QString> kKnownBuiltinProfiles = {
    lit("PLUGINFREE"),
    lit("MOBILE"),
    lit("LeftHalfView"),
    lit("RightHalfView"),
    lit("FisheyeView"),
    lit("Dewarp1"),
    lit("Dewarp2")
};

} // namespace

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
        setInteger(&frameRate, parameterValue);
    else if (parameterName == lit("CompressionLevel"))
        setInteger(&compressionLevel, parameterValue);
    else if (parameterName == lit("Bitrate"))
        setInteger(&bitrateKbps, parameterValue);
    else if (parameterName.endsWith(lit("BitrateControlType")))
        bitrateControl = fromHanwhaString<Qn::BitrateControl>(parameterValue);
    else if (parameterName.endsWith("PriorityType"))
        encodiingPriority = fromHanwhaString<Qn::EncodingPriority>(parameterValue);
    else if (parameterName.endsWith("MaxDynamicGOVLength"))
        setInteger(&maxDynamicGovLength, parameterValue);
    else if (parameterName.endsWith("DynamicGOVLength"))
        setInteger(&dynamicGovLength, parameterValue);
    else if (parameterName.endsWith("MaxGOVLength"))
        setInteger(&maxGovLength, parameterValue);
    else if (parameterName.endsWith("MinGOVLength"))
        setInteger(&minGovLength, parameterValue);
    else if (parameterName.endsWith("GOVLength"))
        setInteger(&govLength, parameterValue);
    else if (parameterName == lit("H264.Profile"))
        codecProfile = parameterValue.trimmed();
    else if (parameterName == lit("H265.Profile"))
        codecProfile = parameterValue.trimmed();
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

bool HanwhaVideoProfile::isBuiltinProfile() const
{
    return this->fixed || kKnownBuiltinProfiles.contains(this->name);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
