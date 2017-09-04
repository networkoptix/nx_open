#if defined(ENABLE_HANWHA)

#include "hanwha_resource.h"
#include "hanwha_utils.h"
#include "hanwha_common.h"
#include "hanwha_request_helper.h"
#include "hanwha_stream_reader.h"
#include "hanwha_ptz_controller.h"

#include <utils/xml/camera_advanced_param_reader.h>
#include <core/resource/camera_advanced_param.h>

#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const QString kAdvancedParametersTemplateFile =
    lit("c:\\develop\\nx_vms\\common\\static-resources\\camera_advanced_params\\hanwha.xml");

} // namespace

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
    const QSet<QString> ids = {id};
    QnCameraAdvancedParamValueList params;
    bool result = getParamsPhysical(ids, params);

    if (!result)
        return false;

    if (params.isEmpty())
        return false;

    if (params[0].id != id)
        return false;

    value = params[0].value;
    return true;
}

bool HanwhaResource::getParamsPhysical(
    const QSet<QString> &idList,
    QnCameraAdvancedParamValueList& result)
{
    using ParameterList = std::vector<QString>;
    using SubmenuMap = std::map<QString, ParameterList>;
    using CgiMap = std::map<QString, SubmenuMap>;

    CgiMap requests;
    std::map<QString, QString> parameterNameToId;

    for (const auto& id: idList)
    {
        const auto parameter = m_advancedParameters.getParameterById(id);
        if (parameter.dataType == QnCameraAdvancedParameter::DataType::Button)
            continue;

        const auto info = advancedParameterInfo(id);
        if (!info || !info->isValid())
            continue;
        
        const auto cgi = info->cgi();
        const auto submenu = info->submenu();
        const auto parmeterName = info->parameterName();

        requests[cgi][submenu].push_back(parmeterName);
        parameterNameToId[parmeterName] = id;
    }

    for (const auto& cgiEntry: requests)
    {
        const auto cgi = cgiEntry.first;
        const auto submenuMap = cgiEntry.second;

        for (const auto& submenuEntry: submenuMap)
        {
            auto submenu = submenuEntry.first;
            HanwhaRequestHelper helper(toSharedPointer(this));
            auto response = helper.view(
                lit("%1/%2").arg(cgi).arg(submenu),
                {{kHanwhaChannelProperty, QString::number(getChannel())}});

            if (!response.isSuccessful())
                continue;

            for (const auto& parameterName: requests[cgi][submenu])
            {
                QString parameterString;
                auto parameterId = parameterNameToId[parameterName];
                auto info = advancedParameterInfo(parameterId);
                if (!info)
                    continue;

                if (!info->isChannelIndependent())
                    parameterString += kHanwhaChannelPropertyTemplate.arg(getChannel());

                auto profile = profileByRole(info->profileDependency());
                if (profile != kHanwhaInvalidProfile)
                    parameterString += lit(".Profile.%1").arg(profile);

                parameterString += lit(".%1").arg(parameterName);

                auto value = response.parameter<QString>(parameterString);
                if (!value)
                    continue;

                result.push_back(QnCameraAdvancedParamValue(parameterId, *value));
            }
        }
    }

    return true;
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

    result = initPtz();
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
    if (m_ptzCapabilities == Ptz::NoPtzCapabilities)
        return nullptr;

    auto controller = new HanwhaPtzController(toSharedPointer(this));
    controller->setPtzCapabilities(m_ptzCapabilities);
    controller->setPtzLimits(m_ptzLimits);

    return controller;
}

CameraDiagnostics::Result HanwhaResource::initMedia()
{
    HanwhaRequestHelper helper(toSharedPointer(this));

    const auto profiles = helper.view(lit("media/videoprofile"));
    if (!profiles.isSuccessful())
    {
        return error(
            profiles,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile/view"),
                profiles.errorString()));
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
        return error(
            mediaAttributes,
            CameraDiagnostics::RequestFailedResult(
                lit("attributes/Media"),
                lit("Invalid response")));
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

    return createNxProfiles();
}

CameraDiagnostics::Result HanwhaResource::initIo()
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    
    const auto parameters = helper.fetchCgiParameters(lit("eventstatus"));
    if (!parameters.isValid())
        return CameraDiagnostics::NoErrorResult();

    QnIOPortDataList ioPorts;

    const auto alarmInputs = parameters.parameter(lit("eventstatus/eventstatus/check/AlarmInput"));
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

    const auto alarmOutputs = parameters.parameter(lit("eventstatus/eventstatus/check/AlarmOutput"));
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

CameraDiagnostics::Result HanwhaResource::initPtz()
{
    HanwhaRequestHelper helper(toSharedPointer(this));

    const auto attributes = helper.fetchAttributes(lit("attributes/PTZSupport"));
    m_ptzCapabilities = Ptz::NoPtzCapabilities;

    if (!attributes.isValid())
    {
        NX_WARNING(this, lit("Can not fetch PTZ capabilities."));
        return CameraDiagnostics::NoErrorResult();
    }

    for (const auto& attributeToCheck: kHanwhaPtzCapabilityAttributes)
    {
        const auto& name = attributeToCheck.first;
        const auto& capability = attributeToCheck.second;

        const auto attr = attributes.attribute<bool>(name);
        if (!attr.is_initialized())
            continue;

        if (!attr.get())
            continue;

        m_ptzCapabilities |= capability;
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initAdvancedParameters()
{
    QnCameraAdvancedParams parameters;
    QFile advancedParametersFile(kAdvancedParametersTemplateFile);

    bool result = QnCameraAdvacedParamsXmlParser::readXml(&advancedParametersFile, parameters);
    NX_ASSERT(result, lm("Error while parsing xml: %1").arg(kAdvancedParametersTemplateFile));
    if (!result)
        return CameraDiagnostics::NoErrorResult();

    HanwhaRequestHelper helper(toSharedPointer(this));
    auto cgiParameters = helper.fetchCgiParameters(lit("cgis"));

    for (const auto& id: parameters.allParameterIds())
    {
        const auto parameter = parameters.getParameterById(id);
        HanwhaAdavancedParameterInfo info(parameter);

        if(!info.isValid())
            continue;

        m_advancedParameterInfos.emplace(id, info);
    }
    
    bool success = fillRanges(&parameters, cgiParameters);
    if (!success)
        return CameraDiagnostics::NoErrorResult();

    parameters = filterParameters(parameters);
    
    {
        QnMutexLocker lock(&m_mutex);
        m_advancedParameters = parameters;
    }

    QnCameraAdvancedParamsReader::setParamsToResource(toSharedPointer(this), parameters);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initTwoWayAudio()
{
    HanwhaRequestHelper helper(toSharedPointer(this));

    const auto attributes = helper.fetchAttributes(lit("Media"));
    if (!attributes.isValid())
        return CameraDiagnostics::NoErrorResult();

    const auto maxAudioOutput = attributes.attribute<int>(lit("Media/Limit/MaxAudioOutput"));
    if (!maxAudioOutput.is_initialized())
        return CameraDiagnostics::NoErrorResult();

    if (*maxAudioOutput - 1 < getChannel())
        return CameraDiagnostics::NoErrorResult();

    setCameraCapability(Qn::AudioTransmitCapability, true);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initRemoteArchive()
{
    setCameraCapability(Qn::RemoteArchiveCapability, false);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::createNxProfiles()
{
    int nxPrimaryProfileNumber = kHanwhaInvalidProfile;
    int nxSecondaryProfileNumber = kHanwhaInvalidProfile;
    std::set<HanwhaProfileNumber> profilesToRemove;
    int totalProfileNumber = 0;

    m_profileByRole.clear();

    auto result = findProfiles(
        &nxPrimaryProfileNumber,
        &nxSecondaryProfileNumber,
        &totalProfileNumber,
        &profilesToRemove);

    if (!result)
        return result;

    if (nxPrimaryProfileNumber == kHanwhaInvalidProfile)
    {
        result = createNxProfile(
            Qn::ConnectionRole::CR_LiveVideo,
            &nxPrimaryProfileNumber,
            totalProfileNumber,
            &profilesToRemove);

        if (!result)
            return result;
    }

    if (nxSecondaryProfileNumber == kHanwhaInvalidProfile)
    {
        result = createNxProfile(
            Qn::ConnectionRole::CR_SecondaryLiveVideo,
            &nxSecondaryProfileNumber,
            totalProfileNumber,
            &profilesToRemove);

        if (!result)
            return result;
    }

    m_profileByRole[Qn::ConnectionRole::CR_LiveVideo] = nxPrimaryProfileNumber;
    m_profileByRole[Qn::ConnectionRole::CR_SecondaryLiveVideo] = nxSecondaryProfileNumber;

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::createNxProfile(
    Qn::ConnectionRole role,
    int* outNxProfile,
    int totalProfileNumber,
    std::set<int>* inOutProfilesToRemove)
{
    const int profileToRemove = chooseProfileToRemove(
        totalProfileNumber,
        *inOutProfilesToRemove);

    if (profileToRemove != kHanwhaInvalidProfile)
    { 
        auto result = removeProfile(profileToRemove);
        if (!result)
            return result;

        inOutProfilesToRemove->erase(profileToRemove);
    }

    return createProfile(outNxProfile, role);
}

int HanwhaResource::chooseProfileToRemove(
    int totalProfileNumber,
    const std::set<int>& profilesToRemove) const
{
    auto profile = kHanwhaInvalidProfile;
    if (totalProfileNumber >= maxProfileCount())
    {
        if (profilesToRemove.empty())
            return kHanwhaInvalidProfile;

        profile = *profilesToRemove.cbegin();
    }

    return profile;
}

CameraDiagnostics::Result HanwhaResource::findProfiles(
    int* outPrimaryProfileNumber,
    int* outSecondaryProfileNumber,
    int* totalProfileNumber,
    std::set<int>* profilesToRemoveIfProfilesExhausted)
{
    bool isParametersValid = outPrimaryProfileNumber
        && outSecondaryProfileNumber
        && totalProfileNumber
        && profilesToRemoveIfProfilesExhausted;

    NX_ASSERT(isParametersValid);
    if (!isParametersValid)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Find profile: wrong output parameters provided"));
    }

    NX_ASSERT(profilesToRemoveIfProfilesExhausted);
    if (!profilesToRemoveIfProfilesExhausted)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Find profile: no profiles to remove provided"));
    }

    profilesToRemoveIfProfilesExhausted->clear();
    *outPrimaryProfileNumber = kHanwhaInvalidProfile;
    *outSecondaryProfileNumber = kHanwhaInvalidProfile;

    HanwhaRequestHelper helper(toSharedPointer(this));
    const auto response = helper.view(lit("media/videoprofile"));

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile/view"),
                response.errorString()));
    }

    const auto profileByChannel = parseProfiles(response);
    const auto currentChannelProfiles = profileByChannel.find(getChannel());
    if (currentChannelProfiles == profileByChannel.cend())
        return CameraDiagnostics::NoErrorResult();

    *totalProfileNumber = currentChannelProfiles->second.size();
    for (const auto& entry : currentChannelProfiles->second)
    {
        const auto& profile = entry.second;
        
        if (profile.name == kPrimaryNxProfileName)
            *outPrimaryProfileNumber = profile.number;
        else if (profile.name == kSecondaryNxProfileName)
            *outSecondaryProfileNumber = profile.number;
        else if (!profile.fixed)
            profilesToRemoveIfProfilesExhausted->insert(profile.number);
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::removeProfile(int profileNumber)
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    const auto response = helper.remove(
        lit("media/videoprofile"),
        {
            {kHanwhaChannelProperty, QString::number(getChannel())},
            {kHanwhaProfileNumberProperty, QString::number(profileNumber)}
        });

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile/remove"),
                response.errorString()));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::createProfile(
    int* outProfileNumber,
    Qn::ConnectionRole role)
{
    NX_ASSERT(outProfileNumber);
    if (!outProfileNumber)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Create profile: output profile number is null"));
    }

    HanwhaRequestHelper helper(toSharedPointer(this));
    const auto response = helper.add(
        lit("media/videoprofile"),
        {
            {kHanwhaProfileNameProperty, nxProfileName(role)},
            {kHanwhaEncodingTypeProperty, toHanwhaString(streamCodec(role))}
        });

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile/add"),
                response.errorString()));
    }

    bool success = false;
    *outProfileNumber = response.response()[kHanwhaProfileNumberProperty].toInt(&success);

    if (!success)
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Invalid profile number string"));
    }

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

    const auto parameters = helper.fetchCgiParameters(lit("media"));
    if (!parameters.isValid())
    {
        return error(
            parameters,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile"),
                lit("Request failed")));
    }

    for (const auto& limitParameter: kHanwhaStreamLimitParameters)
    {
        const auto parameter = parameters.parameter(
            lit("media/videoprofile/add_update/%1").arg(limitParameter));

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

CameraDiagnostics::Result HanwhaResource::fetchPtzLimits(QnPtzLimits* outPtzLimits)
{
    // TODO: #dmishin implement;
    return CameraDiagnostics::NoErrorResult();
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

QnCameraAdvancedParams HanwhaResource::filterParameters(
    const QnCameraAdvancedParams& allParameters) const
{
    QSet<QString> supportedIds;
    for (const auto& id: allParameters.allParameterIds())
    {
        const auto& parameter = allParameters.getParameterById(id);

        bool needToCheck = parameter.dataType == QnCameraAdvancedParameter::DataType::Number
            || parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration;

        if (needToCheck && parameter.range.isEmpty())
            continue;
        
        supportedIds.insert(id);
    }

    return allParameters.filtered(supportedIds);
}

bool HanwhaResource::fillRanges(
    QnCameraAdvancedParams* inOutParameters,
    const HanwhaCgiParameters& cgiParameters) const
{
    for (const auto& id : inOutParameters->allParameterIds())
    {
        auto parameter = inOutParameters->getParameterById(id);
        auto info = advancedParameterInfo(id);
        if (!info)
            continue;

        const bool needToFixRange =
            (parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration
            || parameter.dataType == QnCameraAdvancedParameter::DataType::Number)
                && parameter.range.isEmpty();

        if (!needToFixRange)
            continue;
 
        auto range = cgiParameters.parameter(info->rangeParameter());
        if (!range || !range->isValid())
            continue;

        const auto rangeType = range->type();
        if (rangeType == HanwhaCgiParameterType::enumeration)
        {
            const auto possibleValues = range->possibleValues();
            parameter.range = join(fromHanwhaInternalRange(possibleValues), lit(","));
            parameter.internalRange = join(possibleValues, lit(","));
            inOutParameters->updateParameter(parameter);
        }
        else if (rangeType == HanwhaCgiParameterType::integer)
        {
            parameter.range = lit("%1,%2")
                .arg(range->min())
                .arg(range->max());
            inOutParameters->updateParameter(parameter);
        }
    }

    return true;
}

boost::optional<HanwhaAdavancedParameterInfo> HanwhaResource::advancedParameterInfo(
    const QString& id) const
{
    auto itr = m_advancedParameterInfos.find(id);
    if (itr == m_advancedParameterInfos.cend())
        return boost::none;

    return itr->second;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
