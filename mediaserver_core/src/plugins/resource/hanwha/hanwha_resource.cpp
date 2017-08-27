#if defined(ENABLE_HANWHA)

#include "hanwha_resource.h"
#include "hanwha_stream_reader.h"
#include "hanwha_utils.h"
#include "hanwha_request_helper.h"
#include "hanwha_common.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaResource::HanwhaResource()
{

}

HanwhaResource::~HanwhaResource()
{

}

QnAbstractStreamDataProvider* HanwhaResource::createLiveDataProvider()
{
    return new HanwhaStreamReader(toSharedPointer(this));
}

bool HanwhaResource::getParamPhysical(const QString &id, QString &value)
{
    return false;
}

bool HanwhaResource::getParamsPhysical(
    const QSet<QString> &idList,
    QnCameraAdvancedParamValueList& result)
{
    return false;
}

bool HanwhaResource::setParamPhysical(const QString &id, const QString& value)
{
    return false;
}

bool HanwhaResource::setParamsPhysical(
    const QnCameraAdvancedParamValueList &values,
    QnCameraAdvancedParamValueList &result)
{
    return false;
}

int HanwhaResource::maxProfileCount() const
{
    return m_maxProfileCount;
}

CameraDiagnostics::Result HanwhaResource::initInternal()
{
    CameraDiagnostics::Result result = initMedia();
    if (!result)
        return result;
    
    result = initIo();
    if (!result)
        return result;

    result = initTwoWayAudio();
    if (!result)
        return result;

    result = initAdvancedParameters();
    if (!result)
        return result;

    saveParams();
    return result;
}

QnAbstractPtzController* HanwhaResource::createPtzControllerInternal()
{
    return nullptr;
}

CameraDiagnostics::Result HanwhaResource::initMedia()
{
    HanwhaRequestHelper helper(toSharedPointer(this));

    const auto profiles = helper.view(lit("media/videoprofile"));
    if (!profiles.isSucccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/videoprofile/view"),
            profiles.errorString());
    }

    int fixedProfileCount = 0;

    const auto channel = getChannel();
    const auto startSequence = kHanwhaChannelPropertyTemplate.arg(channel);
    for (const auto& entry: profiles.response())
    {
        const bool isFixedProfile = entry.first.startsWith(startSequence)
            && entry.first.endsWith(kHanwhaIsFixedProfileProperty)
            && entry.second == kHanwhaTrue;

        if (isFixedProfile)
            ++fixedProfileCount;
    }

    const auto mediaAttributes = helper.fetchAttributes(lit("attributes/Media"));
    if (!mediaAttributes.isValid())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("attributes/Media"),
            lit("Invalid response"));
    }

    const auto maxProfileCount = mediaAttributes.attribute<int>(
        lit("Media/MaxProfile/%1").arg(channel));

    if (!maxProfileCount.is_initialized())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("No 'MaxProfile' attribute found for channel %1").arg(channel));
    }

    m_maxProfileCount = *maxProfileCount;
    const bool hasDualStreaming = *maxProfileCount - fixedProfileCount > 1;
    const auto result = fetchStreamLimits(&m_streamLimits);
    const auto& frameRates = m_streamLimits.frameRates;

    if (!result)
        return result;

    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, (int)hasDualStreaming);
    setProperty(Qn::MAX_FPS_PARAM_NAME, frameRates[frameRates.size() - 1]);

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initIo()
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    
    const auto parameters = helper.fetchCgiParameters(lit("eventstatus"));
    if (!parameters.isValid())
        return CameraDiagnostics::NoErrorResult();

    QnIOPortDataList ioPorts;

    const auto alarmInputs = parameters.parameter(lit("eventstatus/check/AlarmInput"));
    if (alarmInputs && alarmInputs->isValid())
    {
        const auto inputs = alarmInputs->possibleValues();
        if (!inputs.empty())
            setCameraCapability(Qn::RelayInputCapability, true);

        for (const auto& input: inputs)
        {
            QnIOPortData inputPortData;
            inputPortData.portType = Qn::PT_Input;
            inputPortData.id = lit("HanwhaAlarmInput.%1").arg(input);
            inputPortData.inputName = tr("Alarm Input #%1").arg(input);

            ioPorts.push_back(inputPortData);
        }
    }

    const auto alarmOutputs = parameters.parameter(lit("eventstatus/check/AlarmOutput"));
    if (alarmOutputs && alarmOutputs->isValid())
    {
        const auto outputs = alarmOutputs->possibleValues();
        if (!outputs.empty())
            setCameraCapability(Qn::RelayOutputCapability, true);

        for (const auto& output : outputs)
        {
            QnIOPortData outputPortData;
            outputPortData.portType = Qn::PT_Output;
            outputPortData.id = lit("HanwhaAlarmOutput.%1").arg(output);
            outputPortData.outputName = tr("Alarm Output #%1").arg(output);

            ioPorts.push_back(outputPortData);
        }
    }

    setIOPorts(ioPorts);

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initAdvancedParameters()
{
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initTwoWayAudio()
{
    HanwhaRequestHelper helper(toSharedPointer(this));

    const auto parameters = helper.fetchAttributes(lit("Media"));
    if (!parameters.isValid())
        return CameraDiagnostics::NoErrorResult();

    setCameraCapability(Qn::AudioTransmitCapability, true);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initRemoteArchive()
{
    setCameraCapability(Qn::RemoteArchiveCapability, true);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::fetchStreamLimits(HanwhaStreamLimits* outStreamLimits)
{
    NX_ASSERT(outStreamLimits);
    if (!outStreamLimits)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Fetch stream limits: no output limits provided"));
    }

    HanwhaRequestHelper helper(toSharedPointer(this));

    const auto parameters = helper.fetchCgiParameters(lit("media/videoprofile"));
    if (!parameters.isValid())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/videoprofile"),
            lit("Request failed"));
    }

    for (const auto& limitParameter: kHanwhaStreamLimitParameters)
    {
        const auto parameter = parameters.parameter(
            lit("videoprofile/add_update/%1").arg(limitParameter));

        if (!parameter.is_initialized())
            continue;

        outStreamLimits->setLimit(*parameter);
    }

    if (!outStreamLimits->isValid())
        return CameraDiagnostics::CameraInvalidParams(lit("Can not fetch stream limits"));

    sortResolutions(&outStreamLimits->resolutions);

    return CameraDiagnostics::NoErrorResult();
}

void HanwhaResource::sortResolutions(std::vector<QSize>* resolutions) const
{
    std::sort(
        resolutions->begin(),
        resolutions->end(),
        [](const QSize& f, const QSize& s)
        { 
            return f.width() * f.height() > s.width() * s.height();
        });
}

AVCodecID HanwhaResource::streamCodec(Qn::ConnectionRole role) const
{
    QString codecString;
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamGovLengthParamName
        : Qn::kSecondaryStreamGovLengthParamName;

    codecString = getProperty(Qn::kPrimaryStreamResolutionParamName);
    if (codecString.isEmpty())
        return defaultCodecForStream(role);

    return fromHanwhaString<AVCodecID>(codecString);
}

QSize HanwhaResource::streamResolution(Qn::ConnectionRole role) const
{
    QString resolutionString;
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamResolutionParamName
        : Qn::kSecondaryStreamCodecParamName;

    resolutionString = getProperty(Qn::kPrimaryStreamResolutionParamName);
    if (resolutionString.isEmpty())
        return defaultResolutionForStream(role);

    return fromHanwhaString<QSize>(resolutionString);
}

int HanwhaResource::streamGovLength(Qn::ConnectionRole role) const
{
    QString govLengthString;
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamGovLengthParamName
        : Qn::kSecondaryStreamGovLengthParamName;

    govLengthString = getProperty(Qn::kPrimaryStreamResolutionParamName);
    if (govLengthString.isEmpty())
        return defaultGovLengthForStream(role);

    return fromHanwhaString<int>(govLengthString);
}

int HanwhaResource::closestFrameRate(Qn::ConnectionRole role, int desiredFrameRate) const
{
    int diff = std::numeric_limits<int>::max();
    int outFrameRate = desiredFrameRate;
    for (const auto& fps: m_streamLimits.frameRates)
    {
        auto currentDiff = std::abs(fps - desiredFrameRate);
        if (currentDiff < diff)
        {
            diff = currentDiff;
            outFrameRate = fps;
        }
    }

    return outFrameRate;
}

int HanwhaResource::profileByRole(Qn::ConnectionRole role) const
{
    auto itr = m_profileByRole.find(role);
    if (itr != m_profileByRole.cend())
        return itr->second;

    return kHanwhaInvalidProfile;
}

void HanwhaResource::setProfileForRole(Qn::ConnectionRole role, int profileNumber)
{
    QnMutexLocker lock(&m_mutex);
    m_profileByRole[role] = profileNumber;
}

AVCodecID HanwhaResource::defaultCodecForStream(Qn::ConnectionRole role) const
{
    const auto& codecs = m_streamLimits.codecs;
    
    if (codecs.find(AVCodecID::AV_CODEC_ID_H264) != codecs.cend())
        return AVCodecID::AV_CODEC_ID_H264;
    else if (codecs.find(AVCodecID::AV_CODEC_ID_HEVC) != codecs.cend())
        return AVCodecID::AV_CODEC_ID_HEVC;
    else if (codecs.find(AVCodecID::AV_CODEC_ID_MJPEG) != codecs.cend())
        return AVCodecID::AV_CODEC_ID_MJPEG;

    return AVCodecID::AV_CODEC_ID_H264;
}

QSize HanwhaResource::defaultResolutionForStream(Qn::ConnectionRole role) const
{
    const auto& resolutions = m_streamLimits.resolutions;
    if (resolutions.empty())
        return QSize();

    if (role == Qn::ConnectionRole::CR_LiveVideo)
        return resolutions[0];

    return bestSecondaryResolution(resolutions[0], resolutions);
    //< TODO: #dmishin consider taking this method from Onvif resource
}

int HanwhaResource::defaultGovLengthForStream(Qn::ConnectionRole role) const
{
    return kHanwhaInvalidGovLength;
}

QSize HanwhaResource::bestSecondaryResolution(
    const QSize& primaryResolution,
    const std::vector<QSize>& resolutionList) const
{
    if (primaryResolution.height() == 0)
    {
        NX_WARNING(
            this,
            lit("Primary resolution height is 0. Can not determine secondary resolution"));

        return QSize();
    }

    const auto primaryAspectRatio = 
        (double)primaryResolution.width() / primaryResolution.height();

    QSize secondaryResolution;
    double minAspectDiff = std::numeric_limits<double>::max();
    
    for (const auto& res: resolutionList)
    {
        const auto width = res.width();
        const auto height = res.height();

        if (width * height > kHanwhaMaxSecondaryStreamArea)
            continue;

        if (height == 0)
        {
            NX_WARNING(
                this,
                lit("Finding secondary resolution: wrong resolution. Resolution height is 0"));
            continue;
        }

        const auto secondaryAspectRatio = (double)width / height;
        const auto diff = std::abs(primaryAspectRatio - secondaryAspectRatio);

        if (diff < minAspectDiff)
        {
            minAspectDiff = diff;
            secondaryResolution = res;
        }
    }

    if (!secondaryResolution.isNull())
        return secondaryResolution;

    NX_WARNING(
        this,
        lit("Can not find secondary resolution of appropriate size, using the smallest one"));

    return resolutionList[resolutionList.size() -1];
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
