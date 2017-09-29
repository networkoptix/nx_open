#if defined(ENABLE_HANWHA)

#include "hanwha_resource.h"
#include "hanwha_utils.h"
#include "hanwha_common.h"
#include "hanwha_request_helper.h"
#include "hanwha_stream_reader.h"
#include "hanwha_ptz_controller.h"
#include "hanwha_resource_searcher.h"

#include <QtCore/QMap>

#include <plugins/resource/onvif/onvif_audio_transmitter.h>
#include <plugins/plugin_internal_tools.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <camera/camera_pool.h>
#include <common/common_module.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/serialization/json.h>
#include <nx/vms/event/events/events.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>

#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/camera_advanced_param.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const QString kAdvancedParametersTemplateFile = lit(":/camera_advanced_params/hanwha.xml");

static const QString kEncodingTypeProperty = lit("EncodingType");
static const QString kResolutionProperty = lit("Resolution");
static const QString kBitrateControlTypeProperty = lit("BitrateControlType");
static const QString kGovLengthProperty = lit("GOVLength");
static const QString kCodecProfileProperty = lit("Profile");
static const QString kEntropyCodingProperty = lit("EntropyCoding");
static const QString kBitrateProperty = lit("Bitrate");
static const QString kFramePriorityProperty = lit("PriorityType");

static const QString kNvrDeviceType = lit("NVR");
static const QString kHanwhaVideoSourceStateOn = lit("On");

//Taken from Hanwha metadata plugin manifest.json
static const QnUuid kHanwhaInputPortEventId =
    QnUuid(lit("{1BAB8A57-5F19-4E3A-B73B-3641058D46B8}"));

static const std::map<QString, std::map<Qn::ConnectionRole, QString>> kStreamProperties = {
    {kEncodingTypeProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, Qn::kPrimaryStreamCodecParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, Qn::kSecondaryStreamCodecParamName}
    }},
    {kResolutionProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, Qn::kPrimaryStreamResolutionParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, Qn::kSecondaryStreamResolutionParamName}
    }},
    {kBitrateControlTypeProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, Qn::kPrimaryStreamBitrateControlParamName},
        {
            Qn::ConnectionRole::CR_SecondaryLiveVideo,
            Qn::kSecondaryStreamBitrateControlParamName
        }
    }},
    {kGovLengthProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, Qn::kPrimaryStreamGovLengthParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, Qn::kSecondaryStreamGovLengthParamName}
    }},
    {kCodecProfileProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, Qn::kPrimaryStreamCodecProfileParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, Qn::kSecondaryStreamCodecProfileParamName}
    }},
    {kEntropyCodingProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, Qn::kPrimaryStreamEntropyCodingParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, Qn::kSecondaryStreamEntropyCodingParamName}
    }}
};

static const std::set<QString> kHanwhaStreamLimitParameters = {
    lit("Resolution"),
    lit("FrameRate"),
    lit("Bitrate"),
    lit("EncodingType"),
    lit("H264.BitrateControlType"),
    lit("H264.GOVLength"),
    lit("H264.PriorityType"),
    lit("H264.Profile"),
    lit("H264.EntropyCoding"),
    lit("H265.BitrateControlType"),
    lit("H265.GOVLength"),
    lit("H265.PriorityType"),
    lit("H265.Profile"),
    lit("H265.EntropyCoding"),
};

struct UpdateInfo
{
    QString cgi;
    QString submenu;
    QString action;
    int profile;
    bool channelIndependent;

    UpdateInfo& operator=(const UpdateInfo& info) = default;
};

bool operator<(const UpdateInfo& lhs, const UpdateInfo& rhs)
{
    if (lhs.cgi != rhs.cgi)
        return lhs.cgi < rhs.cgi;

    if (lhs.submenu != rhs.submenu)
        return lhs.submenu < rhs.submenu;

    if (lhs.action != rhs.action)
        return lhs.action < rhs.action;

    if (lhs.profile != rhs.profile)
        return lhs.profile < rhs.profile;

    if (lhs.channelIndependent != rhs.channelIndependent)
        return lhs.channelIndependent < rhs.channelIndependent;

    return false;
}

struct GroupParameterInfo
{
    GroupParameterInfo() = default;

    GroupParameterInfo(
        const QString& parameterValue,
        const QString& name,
        const QString& lead,
        const QString& condition)
        :
        value(parameterValue),
        groupName(name),
        groupLead(lead),
        groupIncludeCondition(condition)
    {};

    QString value;
    QString groupName;
    QString groupLead;
    QString groupIncludeCondition;
};

} // namespace

HanwhaResource::~HanwhaResource()
{
    // TODO: #dmishin don't forget about it.
}

QnAbstractStreamDataProvider* HanwhaResource::createLiveDataProvider()
{
    return new HanwhaStreamReader(toSharedPointer(this));
}

bool HanwhaResource::getParamPhysical(const QString &id, QString &value)
{
    QnCameraAdvancedParamValueList params;
    bool result = getParamsPhysical({id}, params);

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
    std::multimap<QString, QString> parameterNameToId;

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
        const auto parameterName = info->parameterName();

        requests[cgi][submenu].push_back(parameterName);
        parameterNameToId.emplace(parameterName, id);
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
                const auto idRange = parameterNameToId.equal_range(parameterName);
                for (auto itr = idRange.first; itr != idRange.second; ++itr)
                {
                    QString parameterString;
                    const auto parameterId = itr->second;
                    const auto parameter = m_advancedParameters.getParameterById(parameterId);
                    const auto info = advancedParameterInfo(parameterId);
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
                        value = defaultValue(parameterString.split(L'.').last(), info->profileDependency());
                    if (!value)
                        continue;


                    result.push_back(
                        QnCameraAdvancedParamValue(
                            parameterId,
                            fromHanwhaAdvancedParameterValue(parameter, *info, *value)));
                }
            }
        }
    }

    return true;
}

bool HanwhaResource::setParamPhysical(const QString &id, const QString& value)
{
    QnCameraAdvancedParamValueList result;
    return setParamsPhysical(
        {QnCameraAdvancedParamValue(id, value)},
        result);
}

bool HanwhaResource::setParamsPhysical(
    const QnCameraAdvancedParamValueList &values,
    QnCameraAdvancedParamValueList &result)
{
    using ParameterMap = std::map<QString, QString>;
    using SubmenuMap = std::map<QString, ParameterMap>;
    using CgiMap = std::map<QString, SubmenuMap>;

    bool reopenPrimaryStream = false;
    bool reopenSecondaryStream = false;

    std::map<UpdateInfo, ParameterMap> requests;

    const auto buttonParameter = findButtonParameter(values);
    if (buttonParameter)
    {
        const auto buttonInfo = advancedParameterInfo(buttonParameter->id);
        if (!buttonInfo)
            return false;

        const bool success = executeCommand(*buttonParameter);
        const auto streamsToReopen = buttonInfo->streamsToReopen();
        reopenStreams(
            streamsToReopen.contains(Qn::ConnectionRole::CR_LiveVideo),
            streamsToReopen.contains(Qn::ConnectionRole::CR_SecondaryLiveVideo));

        return success;
    }

    const auto filteredParameters = filterGroupParameters(values);
    for (const auto& value: filteredParameters)
    {
        UpdateInfo updateInfo;
        const auto info = advancedParameterInfo(value.id);
        if (!info)
            continue;

        const auto parameter = m_advancedParameters.getParameterById(value.id);
        const auto resourceProperty = info->resourceProperty();
        
        if (!resourceProperty.isEmpty())
            setProperty(resourceProperty, value.value);

        updateInfo.cgi = info->cgi();
        updateInfo.submenu = info->submenu();
        updateInfo.action = info->updateAction();
        updateInfo.channelIndependent = info->isChannelIndependent();
        updateInfo.profile = profileByRole(info->profileDependency());

        if (updateInfo.profile == profileByRole(Qn::ConnectionRole::CR_LiveVideo))
            reopenPrimaryStream = true;

        if (updateInfo.profile == profileByRole(Qn::ConnectionRole::CR_SecondaryLiveVideo))
            reopenSecondaryStream = true;

        requests[updateInfo][info->parameterName()] = toHanwhaAdvancedParameterValue(
            parameter,
            *info,
            value.value);
    }
    if (reopenPrimaryStream || reopenSecondaryStream)
        initMediaStreamCapabilities();
    saveParams();

    bool success = true;
    for (const auto& request: requests)
    {
        const auto requestCommon = request.first;
        auto requestParameters = request.second;

        if (!requestCommon.channelIndependent)
            requestParameters[kHanwhaChannelProperty] = QString::number(getChannel());

        if (requestCommon.profile != kHanwhaInvalidProfile)
        {
            requestParameters[kHanwhaProfileNumberProperty] 
                = QString::number(requestCommon.profile);
        }

        HanwhaRequestHelper helper(toSharedPointer(this));
        const auto response = helper.doRequest(
            requestCommon.cgi,
            requestCommon.submenu,
            requestCommon.action,
            requestParameters);

        if (!response.isSuccessful())
        {
            success = false;
            continue;
        }
    }

    if (success)
        result = values;

    reopenStreams(reopenPrimaryStream, reopenSecondaryStream);

    return success;
}

QnIOPortDataList HanwhaResource::getRelayOutputList() const
{
    QnIOPortDataList result;
    for (const auto& entry: m_ioPortTypeById)
    {
        if (entry.portType == Qn::PT_Output)
            result.push_back(entry);
    }

    return result;
}

QnIOPortDataList HanwhaResource::getInputPortList() const
{
    QnIOPortDataList result;
    for (const auto& entry : m_ioPortTypeById)
    {
        if (entry.portType == Qn::PT_Input)
            result.push_back(entry);
    }

    return result;
}

bool HanwhaResource::setRelayOutputState(
    const QString& ouputId,
    bool activate,
    unsigned int autoResetTimeoutMs)
{
    const auto info = portInfoFromId(ouputId);
    const auto state = activate ? lit("On") : lit("Off");

    HanwhaRequestHelper::Parameters parameters = 
        {{lit("%1.%2.State").arg(info.prefix).arg(info.number), state}};

    if (info.submenu == lit("alarmoutput"))
    {
        parameters.emplace(
            lit("%1.%2.ManualDuration")
                .arg(info.prefix)
                .arg(info.number),
            lit("Always"));
    }

    HanwhaRequestHelper helper(toSharedPointer(this));
    const auto response = helper.control(
        lit("io/%1").arg(info.submenu),
        parameters);

    return response.isSuccessful();
}

bool HanwhaResource::startInputPortMonitoringAsync(
    std::function<void(bool)>&& completionHandler)
{
    m_areInputPortsMonitored = true;
    return true;
}

void HanwhaResource::stopInputPortMonitoringAsync()
{
    m_areInputPortsMonitored = false;
}

bool HanwhaResource::isInputPortMonitored() const
{
    return m_areInputPortsMonitored;
}

bool HanwhaResource::captureEvent(const nx::vms::event::AbstractEventPtr& event)
{
    if (!m_areInputPortsMonitored)
        return false;
    
    const auto analyticsEvent = event.dynamicCast<nx::vms::event::AnalyticsSdkEvent>();
    if (!analyticsEvent)
        return false;

    const auto parameters = analyticsEvent->getRuntimeParams();
    if (parameters.analyticsEventId() != kHanwhaInputPortEventId)
        return false;

    emit cameraInput(
        toSharedPointer(this),
        analyticsEvent->auxiliaryData(),
        analyticsEvent->getToggleState() == nx::vms::event::EventState::active,
        parameters.eventTimestampUsec);

    return true;
}

bool HanwhaResource::isEventComesFromAnalyticsDriver(nx::vms::event::EventType eventType) const
{
    return base_type::isEventComesFromAnalyticsDriver(eventType)
        || eventType == nx::vms::event::EventType::cameraInputEvent;
}

QString HanwhaResource::sessionKey(
    HanwhaSessionType sessionType,
    bool generateNewOne)
{
    auto discoveryManager = commonModule()->resourceDiscoveryManager();
    if (!discoveryManager)
        return QString();

    auto searcher = dynamic_cast<HanwhaResourceSearcher*>(
        discoveryManager->searcherByManufacture(kHanwhaManufacturerName));

    if (!searcher)
        return QString();

    return searcher->sessionKey(toSharedPointer(this), sessionType, generateNewOne);
}

bool HanwhaResource::isVideoSourceActive()
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    auto response = helper.view(lit("media/videosource"));

    if (!response.isSuccessful())
        return false;

    const auto state = response.parameter<QString>(
        lit("Channel.%1.State").arg(getChannel()));

    if (!state.is_initialized())
        return false;

    return state == kHanwhaVideoSourceStateOn;
}

int HanwhaResource::maxProfileCount() const
{
    return m_maxProfileCount;
}

CameraDiagnostics::Result HanwhaResource::initInternal()
{
    const auto result = init();
    if (result.errorCode == CameraDiagnostics::ErrorCode::notAuthorised)
        setStatus(Qn::Unauthorized);

    return result;
}

CameraDiagnostics::Result HanwhaResource::init()
{
    CameraDiagnostics::Result result = initSystem();
    if (!result)
        return result;

    result = initAttributes();
    if (!result)
        return result;

    result = initMedia();
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

    initMediaStreamCapabilities();

    saveParams();
    return result;
}

void HanwhaResource::initMediaStreamCapabilities()
{
    m_capabilities.streamCapabilities[Qn::ConnectionRole::CR_LiveVideo] =
        mediaCapabilityForRole(Qn::ConnectionRole::CR_LiveVideo);
    m_capabilities.streamCapabilities[Qn::ConnectionRole::CR_SecondaryLiveVideo] =
        mediaCapabilityForRole(Qn::ConnectionRole::CR_SecondaryLiveVideo);
    setProperty(
        nx::media::kCameraMediaCapabilityParamName,
        QString::fromLatin1(QJson::serialized(m_capabilities)));
}

nx::media::CameraStreamCapability HanwhaResource::mediaCapabilityForRole(Qn::ConnectionRole role) const
{
    nx::media::CameraStreamCapability capability;
    if (m_isNvr)
    {
        capability.maxFps = 1;
        return capability;
    }

    const auto codec = streamCodec(role);
    const auto resolution = streamResolution(role);
    const auto bitrateControlType = streamBitrateControl(role);

    const auto limits = m_codecInfo.limits(
        getChannel(),
        codec,
        lit("General"),
        resolution);

    capability.minBitrateKbps = bitrateControlType == Qn::BitrateControl::cbr
        ? limits->minCbrBitrate
        : limits->minVbrBitrate;

    capability.maxBitrateKbps = bitrateControlType == Qn::BitrateControl::cbr
        ? limits->maxCbrBitrate
        : limits->maxVbrBitrate;

    capability.defaultBitrateKbps = bitrateControlType == Qn::BitrateControl::cbr
        ? limits->defaultCbrBitrate
        : limits->defaultVbrBitrate;

    capability.defaultFps = limits->defaultFps > 0 ? limits->defaultFps : limits->maxFps;
    capability.maxFps = limits->maxFps;

    return capability;
}

QnAbstractPtzController* HanwhaResource::createPtzControllerInternal()
{
    if (m_ptzCapabilities == Ptz::NoPtzCapabilities)
        return nullptr;

    auto controller = new HanwhaPtzController(toSharedPointer(this));
    controller->setPtzCapabilities(m_ptzCapabilities);
    controller->setPtzLimits(m_ptzLimits);
    controller->setPtzTraits(m_ptzTraits);

    return controller;
}

CameraDiagnostics::Result HanwhaResource::initSystem()
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    auto response = helper.view(lit("system/deviceinfo"));

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::CameraInvalidParams(
                lit("Can not fetch device information")));
    }

    const auto firmware = response.parameter<QString>(lit("FirmwareVersion"));
    const auto deviceType = response.parameter<QString>(lit("DeviceType"));

    if (!deviceType || deviceType->isEmpty())
        m_isNvr = false;
    else
        m_isNvr = (*deviceType).trimmed() == kNvrDeviceType;
    
    if (!firmware.is_initialized())
        return CameraDiagnostics::NoErrorResult();

    if (!firmware->isEmpty())
        setFirmware(firmware.get());

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initAttributes()
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    m_attributes = helper.fetchAttributes(lit("attributes"));
    
    if (!m_attributes.isValid())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Camera attributes are invalid"));
    }

    m_cgiParameters = helper.fetchCgiParameters(lit("cgis"));

    if (!m_cgiParameters.isValid())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Camera cgi parameters are invalid"));
    }

    return CameraDiagnostics::NoErrorResult();

}

CameraDiagnostics::Result HanwhaResource::initMedia()
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    const auto profiles = helper.view(lit("media/videoprofile"));
    bool isCriticalError = !profiles.isSuccessful()
        && profiles.errorCode() != kHanwhaConfigurationNotFoundError;

    if (isCriticalError)
    {
        return error(
            profiles,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile/view"),
                profiles.errorString()));
    }

    bool hasDualStreaming = false;
    const auto channel = getChannel();
    
    if (!m_isNvr)
    {
        int fixedProfileCount = 0;

        const auto channelPrefix = kHanwhaChannelPropertyTemplate.arg(channel);
        for (const auto& entry: profiles.response())
        {
            const bool isFixedProfile = entry.first.startsWith(channelPrefix)
                && entry.first.endsWith(kHanwhaIsFixedProfileProperty)
                && entry.second == kHanwhaTrue;

            if (isFixedProfile)
                ++fixedProfileCount;
        }

        const auto maxProfileCount = m_attributes.attribute<int>(
            lit("Media/MaxProfile/%1").arg(channel));

        if (!maxProfileCount.is_initialized())
        {
            return CameraDiagnostics::CameraInvalidParams(
                lit("No 'MaxProfile' attribute found for channel %1").arg(channel));
        }

        m_maxProfileCount = *maxProfileCount;
        hasDualStreaming = m_maxProfileCount - fixedProfileCount > 1;
        auto result = fetchStreamLimits(&m_streamLimits);

        if (!result)
            return result;

        result = fetchCodecInfo(&m_codecInfo);

        if (!result)
            return result;

        auto primaryStreamLimits = m_codecInfo.limits(
            getChannel(),
            defaultCodecForStream(Qn::ConnectionRole::CR_LiveVideo),
            lit("General"),
            streamResolution(Qn::ConnectionRole::CR_LiveVideo));

        if (!primaryStreamLimits)
        {
            return CameraDiagnostics::CameraInvalidParams(
                lit("Can not fetch primary stream limits."));
        }

        result = createNxProfiles();
        if (!result)
            return result;
    }
    else if (!isVideoSourceActive())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Video source is not active"));
    }

    const bool hasAudio = m_attributes.attribute<int>(
        lit("Media/MaxAudioInput/%1").arg(channel)) > 0;

    m_capabilities.hasAudio = hasAudio;
    m_capabilities.hasDualStreaming = hasDualStreaming;

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initIo()
{
    QnIOPortDataList ioPorts;

    const auto maxAlarmInputs = m_attributes.attribute<int>(
        lit("Eventsource/MaxAlarmInput"));

    const auto maxAlarmOutputs = m_attributes.attribute<int>(
        lit("IO/MaxAlarmOutput"));

    const auto maxAuxDevices = m_attributes.attribute<int>(
        lit("IO/MaxAux"));
    
    if (maxAlarmInputs.is_initialized() && *maxAlarmInputs > 0)
    {
        setCameraCapability(Qn::RelayInputCapability, true);
        for (auto i = 1; i <= maxAlarmInputs.get(); ++i)
        {
            QnIOPortData inputPortData;
            inputPortData.portType = Qn::PT_Input;
            inputPortData.id = lit("AlarmInput.%1").arg(i);
            inputPortData.inputName = tr("Alarm Input #%1").arg(i);

            m_ioPortTypeById[inputPortData.id] = inputPortData;
            ioPorts.push_back(inputPortData);
        }
    }

    if (maxAlarmOutputs.is_initialized() && *maxAlarmOutputs > 0)
    {
        setCameraCapability(Qn::RelayOutputCapability, true);
        for (auto i = 1; i <= maxAlarmOutputs.get(); ++i)
        {
            QnIOPortData outputPortData;
            outputPortData.portType = Qn::PT_Output;
            outputPortData.id = lit("AlarmOutput.%1").arg(i);
            outputPortData.outputName = tr("Alarm Output #%1").arg(i);

            m_ioPortTypeById[outputPortData.id] = outputPortData;
            ioPorts.push_back(outputPortData);
        }
    }

    if (maxAuxDevices.is_initialized() && *maxAuxDevices > 0)
    {
        setCameraCapability(Qn::RelayOutputCapability, true);
        for (auto i = 1; i <= maxAuxDevices.get(); ++i)
        {
            QnIOPortData outputPortData;
            outputPortData.portType = Qn::PT_Output;
            outputPortData.id = lit("Aux.%1").arg(i);
            outputPortData.outputName = tr("Auxiliary Device #%1").arg(i);

            m_ioPortTypeById[outputPortData.id] = outputPortData;
            ioPorts.push_back(outputPortData);
        }
    }

    setIOPorts(ioPorts);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initPtz()
{
    removeProperty(Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME);

    m_ptzCapabilities = Ptz::NoPtzCapabilities;
    for (const auto& attributeToCheck: kHanwhaPtzCapabilityAttributes)
    {
        const auto& name = lit("PTZSupport/%1/%2")
            .arg(attributeToCheck.first)
            .arg(getChannel());

        const auto& capability = attributeToCheck.second;

        const auto attr = m_attributes.attribute<bool>(name);
        if (!attr.is_initialized())
            return CameraDiagnostics::NoErrorResult();

        if (!attr.get())
            continue;

        m_ptzCapabilities |= capability;
        if (capability == Ptz::NativePresetsPtzCapability)
            m_ptzCapabilities |= Ptz::PresetsPtzCapability;
    }

    if (m_ptzCapabilities ==Ptz::NoPtzCapabilities)
        return CameraDiagnostics::NoErrorResult();

    if ((m_ptzCapabilities & Ptz::AbsolutePtzCapabilities) == Ptz::AbsolutePtzCapabilities)
        m_ptzCapabilities |= Ptz::DevicePositioningPtzCapability;


    auto autoFocusParameter = m_cgiParameters.parameter(lit("image/focus/control/Mode"));

    if (!autoFocusParameter)
        return CameraDiagnostics::NoErrorResult();

    auto possibleValues = autoFocusParameter->possibleValues();
    if (possibleValues.contains(lit("AutoFocus")))
    {
        m_ptzCapabilities |= Ptz::AuxilaryPtzCapability;
        m_ptzTraits.push_back(Ptz::ManualAutoFocusPtzTrait);
    }

    auto maxPresetParameter = m_attributes.attribute<int>(
        lit("PTZSupport/MaxPreset/%1").arg(getChannel()));

    if (maxPresetParameter)
        m_ptzLimits.maxPresetNumber = maxPresetParameter.get();


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

    for (const auto& id: parameters.allParameterIds())
    {
        const auto parameter = parameters.getParameterById(id);
        HanwhaAdavancedParameterInfo info(parameter);

        if(!info.isValid())
            continue;

        m_advancedParameterInfos.emplace(id, info);
    }
    
    bool success = fillRanges(&parameters, m_cgiParameters);
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
    const auto channel = getChannel();

    const auto codec = m_cgiParameters.parameter(
        lit("media/audiooutput/set/DecodingType"));

    if (!codec || codec->possibleValues().isEmpty())
        return CameraDiagnostics::NoErrorResult();

    const auto bitrateParam = m_cgiParameters.parameter(lit("media/audiooutput/set/Bitrate"));
    int bitrateKbps = 0;
    if (bitrateParam && !bitrateParam->possibleValues().isEmpty())
        bitrateKbps = bitrateParam->possibleValues()[0].toInt();

    m_audioTransmitter.reset(new OnvifAudioTransmitter(this));
    if (bitrateKbps > 0)
        m_audioTransmitter->setBitrateKbps(bitrateKbps);

#if 0
    // Not used so far. Our devices can't change codec, so we need to use current value only.

    QnAudioFormat audioFormat;

    const auto sampleRate = m_cgiParameters.parameter(lit("media/audiooutput/set/SampleRate"));
    int maxSampleRate = 0;
    if (sampleRate)
    {
        for (const auto& value: sampleRate->possibleValues())
            maxSampleRate = std::max(maxSampleRate, value.toInt());
    }

    const auto channelsMode = m_cgiParameters.parameter(lit("media/audiooutput/set/Mode"));
    if (channelsMode)
    {
        for (const auto& value: channelsMode->possibleValues())
        {
            if (value == "Stereo")
                audioFormat.setChannelCount(2);
        }
    }

    for (const auto& codec: codec->possibleValues())
    {
        audioFormat.setCodec(codec);

        if (m_audioTransmitter->isCompatible(audioFormat))
        {
            m_audioTransmitter->setOutputFormat(audioFormat);
            if (maxBitrate > 0)
                m_audioTransmitter->setBitrateKbps(maxBitrate);
            if (codec == "G711")
                audioFormat.setSampleRate(8000); //< Sample rate is predefined in RFC for this codec.
            else if (maxSampleRate > 0)
                audioFormat.setSampleRate(maxSampleRate);
            setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
        }
    }
#endif

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

    *totalProfileNumber = (int) currentChannelProfiles->second.size();
    for (const auto& entry : currentChannelProfiles->second)
    {
        const auto& profile = entry.second;
        
        if (profile.name == nxProfileName(Qn::ConnectionRole::CR_LiveVideo))
            *outPrimaryProfileNumber = profile.number;
        else if (profile.name == nxProfileName(Qn::ConnectionRole::CR_SecondaryLiveVideo))
            *outSecondaryProfileNumber = profile.number;
        else if (!profile.isBuiltinProfile())
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
            {kHanwhaEncodingTypeProperty, toHanwhaString(streamCodec(role))},
            {kHanwhaChannelProperty, QString::number(getChannel())}
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

    for (const auto& limitParameter: kHanwhaStreamLimitParameters)
    {
        const auto parameter = m_cgiParameters.parameter(
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

CameraDiagnostics::Result HanwhaResource::fetchCodecInfo(HanwhaCodecInfo* outCodecInfo)
{
    HanwhaRequestHelper helper(toSharedPointer(this));
    auto response = helper.view(
        lit("media/videocodecinfo"),
        HanwhaRequestHelper::Parameters(),
        kHanwhaChannelProperty);

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videocodecinfo"),
                lit("Request failed")));
    }

    *outCodecInfo = HanwhaCodecInfo(response);
    if (!outCodecInfo->isValid())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Video codec info is invalid"));
    }

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
    NX_ASSERT(outPtzLimits, lit("No output ptz limits provided"));

    auto setRange = [this](qreal* outMin, qreal* outMax, const QString& parameterName)
        {
            const auto parameter = m_cgiParameters.parameter(parameterName);
            if (!parameter)
                return false;

            if (parameter->type() != HanwhaCgiParameterType::floating)
                return false;

            std::tie(*outMin, *outMax) = parameter->floatRange();
            return true;
        };

    setRange(
        &outPtzLimits->minPan,
        &outPtzLimits->maxPan,
        lit("ptzcontrol/absolute/Pan"));

    setRange(
        &outPtzLimits->minTilt,
        &outPtzLimits->maxTilt,
        lit("ptzcontrol/absolute/Tilt"));

    setRange(
        &outPtzLimits->minFov,
        &outPtzLimits->maxFov,
        lit("ptzcontrol/absolute/Zoom"));

    // TODO: #dmishin don't forget it
    outPtzLimits->maxPresetNumber;

    return CameraDiagnostics::NoErrorResult();
}

AVCodecID HanwhaResource::streamCodec(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamCodecParamName
        : Qn::kSecondaryStreamCodecParamName;

    QString codecString = getProperty(propertyName);
    if (codecString.isEmpty())
        return defaultCodecForStream(role);

    auto result = fromHanwhaString<AVCodecID>(codecString);
    if (result == AV_CODEC_ID_NONE)
        return defaultCodecForStream(role);

    return result;
}

QString HanwhaResource::streamCodecProfile(AVCodecID codec, Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamCodecProfileParamName
        : Qn::kSecondaryStreamCodecProfileParamName;

    const QString profile = getProperty(propertyName);
    return suggestCodecProfile(codec, role, profile);
}

QSize HanwhaResource::streamResolution(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamResolutionParamName
        : Qn::kSecondaryStreamResolutionParamName;

    QString resolutionString = getProperty(propertyName);
    if (resolutionString.isEmpty())
        return defaultResolutionForStream(role);

    auto result = fromHanwhaString<QSize>(resolutionString);
    if (result.isEmpty())
        return defaultResolutionForStream(role);

    return result;
}

int HanwhaResource::streamFrameRate(Qn::ConnectionRole role, int desiredFps) const
{
    int userDefinedFps = 0;
    if (role == Qn::ConnectionRole::CR_SecondaryLiveVideo)
        userDefinedFps = getProperty(Qn::kSecondaryStreamFpsParamName).toInt();
    return closestFrameRate(role, userDefinedFps ? userDefinedFps : desiredFps);
}

int HanwhaResource::streamGovLength(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamGovLengthParamName
        : Qn::kSecondaryStreamGovLengthParamName;

    QString govLengthString = getProperty(propertyName);
    if (govLengthString.isEmpty())
        return defaultGovLengthForStream(role);
    
    bool success = false;
    const auto result = fromHanwhaString<int>(govLengthString, &success);
    if (!success)
        return kHanwhaInvalidGovLength;

    return result;
}

Qn::BitrateControl HanwhaResource::streamBitrateControl(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamBitrateControlParamName
        : Qn::kSecondaryStreamBitrateControlParamName;

    QString bitrateControlString = getProperty(propertyName);
    if (bitrateControlString.isEmpty())
        return defaultBitrateControlForStream(role);

    auto result = fromHanwhaString<Qn::BitrateControl>(bitrateControlString);
    if (result == Qn::BitrateControl::undefined)
        return defaultBitrateControlForStream(role);

    return result;
}

int HanwhaResource::streamBitrate(Qn::ConnectionRole role, const QnLiveStreamParams& streamParams) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? Qn::kPrimaryStreamBitrateParamName
        : Qn::kSecondaryStreamBitrateParamName;

    const QString bitrateString = getProperty(propertyName);
    int bitrateKbps = bitrateString.toInt();
    if (bitrateKbps == 0)
        bitrateKbps = QnPhysicalCameraResource::suggestBitrateKbps(streamResolution(role), streamParams, role);
    auto streamCapability = cameraMediaCapability().streamCapabilities.value(role);
    return qBound(streamCapability.minBitrateKbps, bitrateKbps, streamCapability.maxBitrateKbps);
}

int HanwhaResource::closestFrameRate(Qn::ConnectionRole role, int desiredFrameRate) const
{
    const auto resolution = streamResolution(role);
    const auto codec = streamCodec(role);
    
    const auto limits = m_codecInfo.limits(
        getChannel(),
        codec,
        lit("General"),
        resolution);

    if (!limits)
        return kHanwhaInvalidFps;

    return qBound(1, desiredFrameRate, limits->maxFps);
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
    return mediaCapabilityForRole(role).maxFps;
}

Qn::BitrateControl HanwhaResource::defaultBitrateControlForStream(Qn::ConnectionRole role) const
{
    return Qn::BitrateControl::vbr;
}

int HanwhaResource::defaultBitrateForStream(Qn::ConnectionRole role) const
{
    const auto resolution = streamResolution(role);
    const auto codec = streamCodec(role);
    const auto bitrateControl = streamBitrateControl(role);

    const auto limits = m_codecInfo.limits(
        getChannel(),
        codec,
        lit("General"),
        resolution);

    if (!limits)
        return kHanwhaInvalidBitrate;

    if (bitrateControl == Qn::BitrateControl::cbr)
        return limits->defaultCbrBitrate;

    return limits->defaultVbrBitrate;
}

Qn::EntropyCoding HanwhaResource::defaultEntropyCodingForStream(Qn::ConnectionRole role) const
{
    const auto codec = defaultCodecForStream(role);

    const auto entropyCodingParameter = m_cgiParameters.parameter(
        lit("media/videoprofile/add_update/%1.EntropyCoding").arg(toHanwhaString(codec)));

    if (!entropyCodingParameter)
        return Qn::EntropyCoding::undefined;

    const auto possibleValues = entropyCodingParameter->possibleValues();
    if (possibleValues.contains(toHanwhaString(Qn::EntropyCoding::cavlc)))
        return Qn::EntropyCoding::cavlc;

    if (possibleValues.contains(toHanwhaString(Qn::EntropyCoding::cabac)))
        return Qn::EntropyCoding::cabac;

    return Qn::EntropyCoding::undefined;
}

QString HanwhaResource::defaultCodecProfileForStream(Qn::ConnectionRole role) const
{
    const auto codec = streamCodec(role);
    
    const auto codecProfileParameter = m_cgiParameters.parameter(
        lit("media/videoprofile/add_update/%1.Profile").arg(toHanwhaString(codec)));

    if (!codecProfileParameter)
        return QString();

    const auto possibleValues = codecProfileParameter->possibleValues();
    if (possibleValues.isEmpty())
        return QString();

    if (possibleValues.contains(lit("High")))
        return lit("High");

    if (possibleValues.contains(lit("Main")))
        return lit("Main");

    return possibleValues.first();
}

int HanwhaResource::defaultFrameRateForStream(Qn::ConnectionRole role) const
{
    if (role == Qn::ConnectionRole::CR_SecondaryLiveVideo)
        closestFrameRate(role, desiredSecondStreamFps());

    return kHanwhaInvalidFps;
}

QString HanwhaResource::defaultValue(const QString& parameter, Qn::ConnectionRole role) const
{
    if (parameter == kEncodingTypeProperty)
        return toHanwhaString(defaultCodecForStream(role));
    else if (parameter == kResolutionProperty)
        return toHanwhaString(defaultResolutionForStream(role));
    else if (parameter == kBitrateControlTypeProperty)
        return toHanwhaString(defaultBitrateControlForStream(role));
    else if (parameter == kGovLengthProperty)
        return QString::number(defaultGovLengthForStream(role));
    else if (parameter == kCodecProfileProperty)
        return defaultCodecProfileForStream(role);
    else if (parameter == kEntropyCodingProperty)
        return toHanwhaString(defaultEntropyCodingForStream(role));
    else if (parameter == kBitrateProperty)
    {
        int bitrateKbps = mediaCapabilityForRole(role).defaultBitrateKbps;
        if (bitrateKbps > 0)
            return QString::number(bitrateKbps);
    }
    else if (parameter == kFramePriorityProperty)
        return lit("FrameRate");

    return QString();
}

QString HanwhaResource::suggestCodecProfile(
    AVCodecID codec,
    Qn::ConnectionRole role,
    const QString& desiredProfile) const
{
    const QStringList* profiles = nullptr;
    if (codec == AV_CODEC_ID_H264)
        profiles = &m_streamLimits.h264Profiles;
    else if (codec == AV_CODEC_ID_HEVC)
        profiles = &m_streamLimits.hevcProfiles;

    if (profiles && profiles->contains(desiredProfile))
        return desiredProfile;

    return defaultCodecProfileForStream(role);
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
        const auto parameter = allParameters.getParameterById(id);
        const auto info = advancedParameterInfo(parameter.id);

        if (!info)
            continue;

        if (info->isService())
        {
            supportedIds.insert(id);
            continue;
        }

        bool needToCheck = parameter.dataType == QnCameraAdvancedParameter::DataType::Number
            || parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration;

        if (needToCheck && parameter.range.isEmpty())
            continue;

        boost::optional<HanwhaCgiParameter> cgiParameter;
        const auto rangeParameter = info->rangeParameter();
        if (rangeParameter.isEmpty())
        {
            cgiParameter = m_cgiParameters.parameter(
                info->cgi(),
                info->submenu(),
                info->updateAction(),
                info->parameterName());
        }
        else
        {
            cgiParameter = m_cgiParameters.parameter(rangeParameter);
        }

        if (!cgiParameter)
            continue;

        supportedIds.insert(id);
    }

    return allParameters.filtered(supportedIds);
}

bool HanwhaResource::fillRanges(
    QnCameraAdvancedParams* inOutParameters,
    const HanwhaCgiParameters& cgiParameters) const
{
    for (const auto& id: inOutParameters->allParameterIds())
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

        if (info->isSpecific())
            addSpecificRanges(&parameter);
 
        auto range = cgiParameters.parameter(info->rangeParameter());
        if (!range || !range->isValid())
            continue;

        const auto rangeType = range->type();
        if (rangeType == HanwhaCgiParameterType::enumeration)
        {
            const auto possibleValues = range->possibleValues();
            parameter.range = fromHanwhaInternalRange(possibleValues).join(lit(","));
            parameter.internalRange = possibleValues.join(lit(","));
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

bool HanwhaResource::addSpecificRanges(
    QnCameraAdvancedParameter* inOutParameter) const
{
    NX_EXPECT(inOutParameter);
    if (!inOutParameter)
        return false;

    const auto info = advancedParameterInfo(inOutParameter->id);
    if (!info)
        return false;

    const auto parameterName = info->parameterName();
    if (parameterName == kHanwhaBitrateProperty)
        return addBitrateRanges(inOutParameter, *info);
    else if (parameterName == kHanwhaFrameRateProperty)
        return addFrameRateRanges(inOutParameter, *info);

    return true;
}

bool HanwhaResource::addBitrateRanges(
    QnCameraAdvancedParameter* inOutParameter,
    const HanwhaAdavancedParameterInfo& info) const
{
    auto createDependencyFunc = [](
        const HanwhaCodecLimits& limits,
        AVCodecID /*codec*/,
        const QSize& /*resolution*/,
        const QString& bitrateControl)
        {
            const int minLimit = bitrateControl == lit("CBR")
                ? limits.minCbrBitrate
                : limits.minVbrBitrate;

            const int maxLimit = bitrateControl == lit("CBR")
                ? limits.maxCbrBitrate
                : limits.maxVbrBitrate;

            QnCameraAdvancedParameterDependency dependency;
            dependency.id = QnUuid::createUuid().toString();
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::Range;
            dependency.range = lit("%1,%2").arg(minLimit).arg(maxLimit);
            return dependency;
        };

    return addDependencies(inOutParameter, info, createDependencyFunc);
}

bool HanwhaResource::addFrameRateRanges(
    QnCameraAdvancedParameter* inOutParameter,
    const HanwhaAdavancedParameterInfo& info) const
{
    auto createDependencyFunc = [](
        const HanwhaCodecLimits& limits,
        AVCodecID /*codec*/,
        const QSize& /*resolution*/,
        const QString& /*bitrateControl*/)
        {
            QnCameraAdvancedParameterDependency dependency;
            dependency.id = QnUuid::createUuid().toString();
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::Range;
            dependency.range = lit("1,%2").arg(limits.maxFps);
            return dependency;
        };

    return addDependencies(inOutParameter, info, createDependencyFunc);
}

bool HanwhaResource::addDependencies(
    QnCameraAdvancedParameter* inOutParameter,
    const HanwhaAdavancedParameterInfo& info,
    CreateDependencyFunc createDependencyFunc) const
{
    NX_EXPECT(inOutParameter);
    if (!inOutParameter)
        return false;

    const auto channel = getChannel();
    const auto codecs = m_codecInfo.codecs(channel);

    const auto streamPrefix = info.profileDependency() == Qn::ConnectionRole::CR_LiveVideo
        ? lit("PRIMARY%")
        : lit("SECONDARY%");

    for (const auto& codec : codecs)
    {
        const auto resolutions = m_codecInfo.resolutions(channel, codec, lit("General"));
        for (const auto& resolution : resolutions)
        {
            auto limits = m_codecInfo.limits(channel, codec, lit("General"), resolution);

            const auto codecString = toHanwhaString(codec);
            QnCameraAdvancedParameterCondition codecCondition;
            codecCondition.type = QnCameraAdvancedParameterCondition::ConditionType::Equal;
            codecCondition.paramId = lit("%1media/videoprofile/EncodingType")
                .arg(streamPrefix);
            codecCondition.value = codecString;

            const auto resolutionString = toHanwhaString(resolution);
            QnCameraAdvancedParameterCondition resolutionCondition;
            resolutionCondition.type =
                QnCameraAdvancedParameterCondition::ConditionType::Equal;
            resolutionCondition.paramId = lit("%1media/videoprofile/Resolution")
                .arg(streamPrefix);
            resolutionCondition.value = resolutionString;

            QStringList bitrateControlTypeList;
            const auto bitrateControlTypes = m_cgiParameters.parameter(
                lit("media/videoprofile/add_update/%1.BitrateControlType")
                    .arg(codecString));

            if (bitrateControlTypes)
                bitrateControlTypeList = bitrateControlTypes->possibleValues();

            if (bitrateControlTypeList.isEmpty())
                bitrateControlTypeList.push_back(lit("VBR"));

            for (const auto& bitrateControlType : bitrateControlTypeList)
            {
                QnCameraAdvancedParameterCondition bitrateControlTypeCondition;
                bitrateControlTypeCondition.type
                    = QnCameraAdvancedParameterCondition::ConditionType::Equal;
                bitrateControlTypeCondition.paramId
                    = lit("%1media/videoprofile/%2.BitrateControlType")
                        .arg(streamPrefix)
                        .arg(codecString);
                bitrateControlTypeCondition.value = bitrateControlType;

                auto dependency = createDependencyFunc(*limits, codec, resolution, bitrateControlType);
                dependency.conditions.push_back(codecCondition);
                dependency.conditions.push_back(resolutionCondition);

                if (codec != AV_CODEC_ID_MJPEG)
                    dependency.conditions.push_back(bitrateControlTypeCondition);

                inOutParameter->dependencies.push_back(dependency);
            }
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

void HanwhaResource::updateToChannel(int value)
{
    QUrl url(getUrl());
    QUrlQuery query(url.query());
    query.removeQueryItem("channel");
    if (value > 0)
        query.addQueryItem("channel", QString::number(value + 1));
    url.setQuery(query);
    setUrl(url.toString());

    QString physicalId = getPhysicalId().split('_')[0];
    setGroupName(getModel());
    setGroupId(physicalId);

    QString suffix = lit("_channel=%1").arg(value + 1);
    if (value > 0)
        physicalId += suffix;
    setPhysicalId(physicalId);

    suffix = lit("-channel %1").arg(value + 1);
    if (value > 0 && !getName().endsWith(suffix))
        setName(getName() + suffix);
}

QString HanwhaResource::toHanwhaAdvancedParameterValue(
    const QnCameraAdvancedParameter& parameter,
    const HanwhaAdavancedParameterInfo& parameterInfo,
    const QString& str) const
{
    const auto parameterType = parameter.dataType;

    if (parameterType == QnCameraAdvancedParameter::DataType::Bool)
    {
        if (str == lit("true") || str == lit("1"))
            return kHanwhaTrue;

        return kHanwhaFalse;
    }
    else if (parameterType == QnCameraAdvancedParameter::DataType::Enumeration)
    {
        if(!parameter.internalRange.isEmpty())
            return parameter.toInternalRange(str);
    }

    return str;
}

QString HanwhaResource::fromHanwhaAdvancedParameterValue(
    const QnCameraAdvancedParameter& parameter,
    const HanwhaAdavancedParameterInfo& parameterInfo,
    const QString& str) const
{
    const auto parameterType = parameter.dataType;
    if (parameterType == QnCameraAdvancedParameter::DataType::Bool)
    {
        if (str == kHanwhaTrue)
            return lit("true");

        return lit("false");
    }
    else if (parameterType == QnCameraAdvancedParameter::DataType::Enumeration)
    {
        if (!parameter.internalRange.isEmpty())
            return parameter.fromInternalRange(str);
    }

    return str;
}

void HanwhaResource::reopenStreams(bool reopenPrimary, bool reopenSecondary)
{
    auto camera = qnCameraPool->getVideoCamera(toSharedPointer(this));
    if (!camera)
        return;

    auto providerHi = camera->getPrimaryReader();
    auto providerLow = camera->getSecondaryReader();

    if (providerHi && providerHi->isRunning())
        providerHi->pleaseReopenStream();

    if (providerLow && providerLow->isRunning())
        providerLow->pleaseReopenStream();
}

int HanwhaResource::suggestBitrate(
    const HanwhaCodecLimits& limits,
    Qn::BitrateControl bitrateControl,
    double coefficient,
    int framerate) const
{
    int defaultBitrate = kHanwhaInvalidBitrate;
    int minBitrate = kHanwhaInvalidBitrate;
    int maxBitrate = kHanwhaInvalidBitrate;
    if (bitrateControl == Qn::BitrateControl::cbr)
    {
        defaultBitrate = limits.defaultCbrBitrate;
        minBitrate = limits.minCbrBitrate;
        maxBitrate = limits.maxCbrBitrate;
    }
    else
    {
        defaultBitrate = limits.defaultVbrBitrate;
        minBitrate = limits.minVbrBitrate;
        maxBitrate = limits.maxVbrBitrate;
    }

    const int bitrate = defaultBitrate * coefficient;

    return qBound(
        (double)minBitrate,
        bitrate * ((double) framerate / limits.defaultFps),
        (double)maxBitrate);
}

bool HanwhaResource::isBitrateInLimits(
    const HanwhaCodecLimits& limits,
    Qn::BitrateControl bitrateControl,
    int bitrate) const
{
    int minBitrate = kHanwhaInvalidBitrate;
    int maxBitrate = kHanwhaInvalidBitrate;
    if (bitrateControl == Qn::BitrateControl::cbr)
    {
        minBitrate = limits.minCbrBitrate;
        maxBitrate = limits.maxCbrBitrate;
    }
    else
    {
        minBitrate = limits.minVbrBitrate;
        maxBitrate = limits.maxVbrBitrate;
    }

    return bitrate <= maxBitrate && bitrate >= minBitrate;
}

QnCameraAdvancedParamValueList HanwhaResource::filterGroupParameters(
    const QnCameraAdvancedParamValueList& values)
{
    using GroupParameterId = QString;

    QnCameraAdvancedParamValueList result;
    QMap<GroupParameterId, GroupParameterInfo> groupParameters;

    // Fill group info if needed and fill group info for group parameters. 
    for (const auto& value: values)
    {
        const auto info = advancedParameterInfo(value.id);
        if (!info)
            continue;

        const auto group = info->group();
        if (group.isEmpty())
        {
            result.push_back(value);
            continue;
        }

        groupParameters[value.id] = 
            GroupParameterInfo(
                value.value,
                group,
                groupLead(group),
                info->groupIncludeCondition());
    }

    // resolve group parameters
    QSet<QString> groupLeadsToFetch;
    QList<QString> parametersToResolve;

    for (const auto& parameterName: groupParameters.keys())
    {
        if (!groupParameters.contains(parameterName))
            continue;

        const auto info = groupParameters.value(parameterName);
        const auto groupLead = info.groupLead;

        if (parameterName == groupLead)
        {
            result.push_back(QnCameraAdvancedParamValue(parameterName, info.value));
            continue;
        }

        if (!groupParameters.contains(groupLead))
        {
            groupLeadsToFetch.insert(groupLead);
            parametersToResolve.push_back(parameterName);
            continue;
        }

        if (info.groupIncludeCondition == groupParameters[groupLead].value)
            result.push_back(QnCameraAdvancedParamValue(parameterName, info.value));
    }

    if (groupLeadsToFetch.isEmpty())
        return result;

    // fetch group leads;
    QnCameraAdvancedParamValueList groupLeadValues;
    bool success = getParamsPhysical(groupLeadsToFetch, groupLeadValues);

    for (const auto& lead: groupLeadValues)
    {
        const auto info = advancedParameterInfo(lead.id);
        if (!info)
            continue;

        groupParameters[lead.id] = GroupParameterInfo(
            lead.value,
            info->group(),
            lead.id,
            info->groupIncludeCondition());
    }

    for (const auto& parameterName: parametersToResolve)
    {
        if (!groupParameters.contains(parameterName))
            continue;

        const auto info = groupParameters.value(parameterName);
        const auto groupLead = info.groupLead;

        if (parameterName == groupLead)
        {
            result.push_back(QnCameraAdvancedParamValue(parameterName, info.value));
            continue;
        }

        if (!groupParameters.contains(groupLead))
            continue;

        if (info.groupIncludeCondition == groupParameters[groupLead].value)
            result.push_back(QnCameraAdvancedParamValue(parameterName, info.value));
    }

    return result;
}

QString HanwhaResource::groupLead(const QString& groupName) const
{
    for (const auto& entry: m_advancedParameterInfos)
    {
        const auto info = entry.second;
        if (info.group() == groupName && info.isGroupLead())
            return info.id();
    }

    return QString();
}

boost::optional<QnCameraAdvancedParamValue> HanwhaResource::findButtonParameter(
    const QnCameraAdvancedParamValueList parameterValues) const
{
    for (const auto& parameterValue: parameterValues)
    {
        const auto parameter = m_advancedParameters.getParameterById(parameterValue.id);
        if (!parameter.isValid())
            return boost::none;

        if (parameter.dataType != QnCameraAdvancedParameter::DataType::Button)
            continue;

        return parameterValue;
    }

    return boost::none;
}

bool HanwhaResource::executeCommand(const QnCameraAdvancedParamValue& command)
{
    const auto parameter = m_advancedParameters.getParameterById(command.id);
    if (!parameter.isValid())
        return false;

    const auto info = advancedParameterInfo(command.id);
    if (!info)
        return false;

    if (info->isService())
        return executeServiceCommand(parameter, *info);

    const auto cgiParameter = m_cgiParameters.parameter(
        info->cgi(),
        info->submenu(),
        info->updateAction(),
        info->parameterName());

    if (!cgiParameter)
        return false;

    const auto possibleValues = cgiParameter->possibleValues();
    const auto requestedParameterValues = info->parameterValue()
        .split(L',');

    QStringList parameterValues;
    for (const auto& requestedValue: requestedParameterValues)
    {
        if (possibleValues.contains(requestedValue))
            parameterValues.push_back(requestedValue);
    }

    std::map<QString, QString> requestParameters;
    if (!parameterValues.isEmpty())
        requestParameters.emplace(info->parameterName(), parameterValues.join(L','));

    HanwhaRequestHelper helper(toSharedPointer(this));
    const auto response = helper.doRequest(
        info->cgi(),
        info->submenu(),
        info->updateAction(),
        requestParameters);

    return response.isSuccessful();
}

bool HanwhaResource::executeServiceCommand(
    const QnCameraAdvancedParameter& parameter,
    const HanwhaAdavancedParameterInfo& info)
{
    if (parameter.id.endsWith(lit("ResetToDefault")))
        return resetProfileToDefault(info.profileDependency());

    return true;
}

bool HanwhaResource::resetProfileToDefault(Qn::ConnectionRole role)
{
    const std::vector<QString> kPropertiesToSet = {
        kEncodingTypeProperty,
        kResolutionProperty,
        kBitrateControlTypeProperty,
        kGovLengthProperty,
        kCodecProfileProperty,
        kEntropyCodingProperty
    };

    std::map<QString, QString> parameters;
    for (const auto& property: kPropertiesToSet)
    {
        const auto propertyDefaultValue = defaultValue(property, role);
        const auto nxProperty = propertyByPrameterAndRole(property, role);
        setProperty(nxProperty, propertyDefaultValue);
    }

    if (role == Qn::ConnectionRole::CR_SecondaryLiveVideo)
    {
        setProperty(Qn::kSecondaryStreamFpsParamName, defaultFrameRateForStream(role));
        setProperty(Qn::kSecondaryStreamBitrateParamName, defaultBitrateForStream(role));
    }

    saveParams();
    return true;
}

QString HanwhaResource::propertyByPrameterAndRole(
    const QString& parameter,
    Qn::ConnectionRole role) const
{
    auto roleMapEntry = kStreamProperties.find(parameter);
    if (roleMapEntry == kStreamProperties.cend())
        return QString();

    auto entry = roleMapEntry->second.find(role);
    if (entry == roleMapEntry->second.cend())
        return QString();

    return entry->second;
}

HanwhaResource::HanwhaPortInfo HanwhaResource::portInfoFromId(const QString& id) const
{
    HanwhaPortInfo result;
    auto split = id.split(L'.');
    if (split.size() != 2)
        return result;

    result.prefix = split[0];
    result.number = split[1];
    result.submenu = result.prefix.toLower();

    return result;
}

bool HanwhaResource::isNvr() const
{
    return m_isNvr;
}

QString HanwhaResource::nxProfileName(Qn::ConnectionRole role) const
{
    const auto nxProfileNameParameter = m_cgiParameters
        .parameter(lit("media/videoprofile/add_update/Name"));

    const auto maxLength = nxProfileNameParameter && nxProfileNameParameter->maxLength() > 0
        ? nxProfileNameParameter->maxLength()
        : kHanwhaProfileNameMaxLength;
        
    auto suffix = role == Qn::ConnectionRole::CR_LiveVideo
        ? kHanwhaPrimaryNxProfileSuffix
        : kHanwhaSecondaryNxProfileSuffix;

    auto appName = QnAppInfo::productNameLong()
        .mid(0, maxLength - suffix.length())
        .remove(QRegExp("[^a-zA-Z]"));

    return appName + suffix;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
