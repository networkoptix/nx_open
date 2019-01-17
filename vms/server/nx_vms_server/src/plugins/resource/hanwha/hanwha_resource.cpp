#include "hanwha_resource.h"
#include "hanwha_utils.h"
#include "hanwha_common.h"
#include "hanwha_request_helper.h"
#include "hanwha_stream_reader.h"
#include "hanwha_ptz_controller.h"
#include "hanwha_resource_searcher.h"
#include "hanwha_shared_resource_context.h"
#include "hanwha_archive_delegate.h"
#include "hanwha_chunk_reader.h"
#include "hanwha_firmware.h"
#include "hanwha_ini_config.h"

#include <QtCore/QMap>

#include <plugins/resource/onvif/onvif_audio_transmitter.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include <camera/camera_pool.h>
#include <camera/video_camera.h>

#include <common/common_module.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/vms/event/events/events.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/vms/server/resource/shared_context_pool.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/vms/server/plugins/resource_data_support/hanwha.h>

#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/camera_advanced_param.h>

#include <api/global_settings.h>

#include <media_server/media_server_module.h>
#include <core/resource_management/resource_data_pool.h>

namespace nx {
namespace vms::server {
namespace plugins {

using namespace nx::core;

namespace {

static const QString kBypassPrefix("Bypass");

enum class PtzOperation
{
    add,
    remove
};

struct PtzDescriptor
{
    PtzDescriptor(Ptz::Capabilities capability, PtzOperation operation = PtzOperation::add):
        capability(capability), operation(operation)
    {
    }

    Ptz::Capabilities capability = Ptz::Capability::NoPtzCapabilities;
    PtzOperation operation = PtzOperation::add;
};

static const std::map<QString, PtzDescriptor> kHanwhaCameraPtzCapabilityDescriptors =
{
    {"Absolute.Pan", PtzDescriptor(Ptz::Capability::ContinuousPanCapability)},
    {"Absolute.Tilt", PtzDescriptor(Ptz::Capability::ContinuousTiltCapability)},
    {"Absolute.Zoom", PtzDescriptor(Ptz::Capability::ContinuousZoomCapability)},
    {"Relative.Pan", PtzDescriptor(Ptz::Capability::RelativePanCapability)},
    {"Relative.Tilt", PtzDescriptor(Ptz::Capability::RelativeTiltCapability)},
    {"Relative.Zoom", PtzDescriptor(Ptz::Capability::RelativeZoomCapability)},
    {"Continuous.Focus", PtzDescriptor(Ptz::Capability::ContinuousFocusCapability)},
    {"Preset", PtzDescriptor(Ptz::Capability::NativePresetsPtzCapability) },
    {"AreaZoom", PtzDescriptor(
        Ptz::Capability::ViewportPtzCapability |
        Ptz::Capability::AbsolutePanCapability |
        Ptz::Capability::AbsoluteTiltCapability |
        Ptz::Capability::AbsoluteZoomCapability) },
    // Native Home command is not implemented yet
    //{lit("Home"), PtzDescriptor(Ptz::Capability::HomePtzCapability)},
    {
        "DigitalPTZ",
        PtzDescriptor(
            Ptz::Capability::ContinuousZoomCapability |
            Ptz::Capability::ContinuousTiltCapability |
            Ptz::Capability::ContinuousPanCapability,
            PtzOperation::remove
        )
    }
};

static const std::map<QString, PtzDescriptor> kHanwhaNvrPtzCapabilityDescriptors =
{
    {"Absolute.Pan", PtzDescriptor(Ptz::Capability::AbsolutePanCapability)},
    {"Absolute.Tilt", PtzDescriptor(Ptz::Capability::AbsoluteTiltCapability)},
    {"Absolute.Zoom", PtzDescriptor(Ptz::Capability::AbsoluteZoomCapability)},
    {"Relative.Pan", PtzDescriptor(Ptz::Capability::RelativePanCapability)},
    {"Relative.Tilt", PtzDescriptor(Ptz::Capability::RelativeTiltCapability)},
    {"Relative.Zoom", PtzDescriptor(Ptz::Capability::RelativeZoomCapability)},
    {"Continuous.Pan", PtzDescriptor(Ptz::Capability::ContinuousPanCapability)},
    {"Continuous.Tilt", PtzDescriptor(Ptz::Capability::ContinuousTiltCapability)},
    {"Continuous.Zoom", PtzDescriptor(Ptz::Capability::ContinuousZoomCapability)},
    {"Continuous.Focus", PtzDescriptor(Ptz::Capability::ContinuousFocusCapability)},
    {"Preset", PtzDescriptor(Ptz::Capability::NativePresetsPtzCapability)},
    {"AreaZoom", PtzDescriptor(Ptz::Capability::ViewportPtzCapability)},
    // Native Home command is not implemented yet
    //{ lit("Home"), PtzDescriptor(Ptz::Capability::HomePtzCapability) },
    {
        "DigitalPTZ",
        PtzDescriptor(
            Ptz::Capability::ContinuousZoomCapability |
            Ptz::Capability::ContinuousTiltCapability |
            Ptz::Capability::ContinuousPanCapability,
            PtzOperation::remove)
    }
};

struct RangeDescriptor
{
    core::ptz::Types ptzTypes;
    QString name;
    QString cgiParameter;
    QString alternativeCgiParameter;

    RangeDescriptor(
        core::ptz::Types ptzTypes,
        QString name,
        QString cgiParameter,
        QString alternativeCgiParameter = QString())
        :
        ptzTypes(ptzTypes),
        name(name),
        cgiParameter(cgiParameter),
        alternativeCgiParameter(alternativeCgiParameter)
    {
    }
};

static const std::vector<RangeDescriptor> kRangeDescriptors = {
    {core::ptz::Type::operational, "Absolute.Pan", "ptzcontrol/absolute/control/Pan"},
    {core::ptz::Type::operational, "Absolute.Tilt", "ptzcontrol/absolute/control/Tilt"},
    {core::ptz::Type::operational, "Absolute.Zoom", "ptzcontrol/absolute/control/Zoom"},
    {
        core::ptz::Type::operational,
        "Relative.Pan",
        "ptzcontrol/relative/control/Pan",
        "ptzcontrol/absolute/control/Pan"
    },
    {
        core::ptz::Type::operational,
        "Relative.Tilt",
        "ptzcontrol/relative/control/Tilt",
        "ptzcontrol/absolute/control/Tilt"
    },
    {
        core::ptz::Type::operational,
        "Relative.Zoom",
        "ptzcontrol/relative/control/Zoom",
        "ptzcontrol/absolute/control/Zoom"
    },
    {
        core::ptz::Type::configurational,
        "Continuous.Pan",
        "image/ptr/control/Pan"
    },
    {
        core::ptz::Type::configurational,
        "Continuous.Tilt",
        "image/ptr/control/Tilt"
    },
    {
        core::ptz::Type::configurational,
        "Continuous.Zoom",
        "image/focus/control/Zoom"
    },
    {
        core::ptz::Type::configurational,
        "Continuous.Rotate",
        "image/ptr/control/Rotate"
    },
    {
        core::ptz::Type::configurational,
        "Continuous.Focus",
        "image/focus/control/Focus"
    }
};

struct PtzTraitDescriptor
{
    QString parameterName;
    QSet<QString> positiveValues;
};

static const std::map<QString, PtzTraitDescriptor> kHanwhaPtzTraitDescriptors = {
    {
        kHanwhaNormalizedSpeedPtzTrait,
        {
            lit("ptzcontrol/continuous/control/NormalizedSpeed"),
            {kHanwhaTrue}
        }
    },
    {
        kHanwhaHas3AxisPtz,
        {
            lit("ptzcontrol/continuous/control/3AxisPTZ"),
            {kHanwhaTrue}
        }
    }
};

static Ptz::Capabilities calculateSupportedPtzCapabilities(
    const std::map<QString, PtzDescriptor>& descriptors,
    const HanwhaAttributes& attributes,
    int channel)
{
    Ptz::Capabilities supportedPtzCapabilities = Ptz::NoPtzCapabilities;
    for (const auto& entry: descriptors)
    {
        const auto& attributeToCheck = entry.first;
        const auto& descriptor = entry.second;
        const auto& capability = descriptor.capability;
        const auto& fullAttributePath = lit("PTZSupport/%1/%2")
            .arg(attributeToCheck)
            .arg(channel);

        const auto attr = attributes.attribute<bool>(fullAttributePath);
        if (!attr || !attr.get())
            continue;

        if (descriptor.operation == PtzOperation::add)
        {
            supportedPtzCapabilities |= capability;
            if (capability == Ptz::NativePresetsPtzCapability)
                supportedPtzCapabilities |= Ptz::PresetsPtzCapability | Ptz::NoNxPresetsPtzCapability;
        }
        else
        {
            supportedPtzCapabilities &= ~capability;
        }
    }

    return supportedPtzCapabilities;
};

static QnPtzLimits calculatePtzLimits(
    const HanwhaAttributes& attributes,
    const HanwhaCgiParameters& parameters,
    int channel)
{
    QnPtzLimits ptzLimits;
    auto panSpeedParameter = parameters.parameter(
        lit("ptzcontrol/continuous/control/Pan"));
    if (panSpeedParameter)
    {
        ptzLimits.minPanSpeed = panSpeedParameter->min();
        ptzLimits.maxPanSpeed = panSpeedParameter->max();
    }
    auto tiltSpeedParameter = parameters.parameter(
        lit("ptzcontrol/continuous/control/Tilt"));
    if (tiltSpeedParameter)
    {
        ptzLimits.minTiltSpeed = tiltSpeedParameter->min();
        ptzLimits.maxTiltSpeed = tiltSpeedParameter->max();
    }
    auto zoomSpeedParameter = parameters.parameter(
        lit("ptzcontrol/continuous/control/Zoom"));
    if (zoomSpeedParameter)
    {
        ptzLimits.minZoomSpeed = zoomSpeedParameter->min();
        ptzLimits.maxZoomSpeed = zoomSpeedParameter->max();
    }

    auto maxPresetParameter = attributes.attribute<int>(
        lit("PTZSupport/MaxPreset/%1").arg(channel));

    if (maxPresetParameter)
        ptzLimits.maxPresetNumber = maxPresetParameter.get();

    return ptzLimits;
};

struct HanwhaConfigurationalPtzDescriptor
{
    QString supportAttribute;
    QString valueParameter;
    Ptz::Capabilities capabilities;
};

static const std::vector<HanwhaConfigurationalPtzDescriptor>
    kHanwhaConfigurationalPtzDescriptors =
    {
        {
            lit("Image/FocusAdjust"),
            lit("image/focus/control/Focus"),
            Ptz::ContinuousFocusCapability
        },
        {
            lit("Image/ZoomAdjust"),
            lit("image/focus/control/Zoom"),
            Ptz::ContinuousZoomCapability
        },
        {
            QString(), //< No attribute is available for PTR.
            lit("image/ptr/control/Pan"),
            Ptz::ContinuousPanCapability
        },
        {
            QString(),
            lit("image/ptr/control/Tilt"),
            Ptz::ContinuousTiltCapability
        },
        {
            QString(),
            lit("image/ptr/control/Rotate"),
            Ptz::ContinuousRotationCapability
        }
    };

static const QString kAdvancedParametersTemplateFile = lit(":/camera_advanced_params/hanwha.xml");

static const QString kEncodingTypeProperty = lit("EncodingType");
static const QString kResolutionProperty = lit("Resolution");
static const QString kBitrateControlTypeProperty = lit("BitrateControlType");
static const QString kGovLengthProperty = lit("GOVLength");
static const QString kCodecProfileProperty = lit("Profile");
static const QString kEntropyCodingProperty = lit("EntropyCoding");
static const QString kBitrateProperty = lit("Bitrate");
static const QString kFramePriorityProperty = lit("PriorityType");

static const QString kHanwhaVideoSourceStateOn = lit("On");
static const int kHanwhaInvalidInputValue = 604;

// Taken from Hanwha metadata plugin manifest.json.
static const QString kHanwhaInputPortEventTypeId = "nx.hanwha.inputPort";

static const std::map<QString, std::map<Qn::ConnectionRole, QString>> kStreamProperties = {
    {kEncodingTypeProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, kPrimaryStreamCodecParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, kSecondaryStreamCodecParamName}
    }},
    {kResolutionProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, kPrimaryStreamResolutionParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, kSecondaryStreamResolutionParamName}
    }},
    {kBitrateControlTypeProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, kPrimaryStreamBitrateControlParamName},
        {
            Qn::ConnectionRole::CR_SecondaryLiveVideo,
            kSecondaryStreamBitrateControlParamName
        }
    }},
    {kGovLengthProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, kPrimaryStreamGovLengthParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, kSecondaryStreamGovLengthParamName}
    }},
    {kCodecProfileProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, kPrimaryStreamCodecProfileParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, kSecondaryStreamCodecProfileParamName}
    }},
    {kEntropyCodingProperty,
    {
        {Qn::ConnectionRole::CR_LiveVideo, kPrimaryStreamEntropyCodingParamName},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, kSecondaryStreamEntropyCodingParamName}
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

static QString physicalIdForChannel(const QString& groupId, int value)
{
    auto id = groupId;
    if (value > 0)
        id += lit("_channel=%1").arg(value + 1);

    return id;
}

} // namespace

HanwhaResource::HanwhaResource(QnMediaServerModule* serverModule): QnPlOnvifResource(serverModule)
{
}

HanwhaResource::~HanwhaResource()
{
    m_timerHolder.terminate();
}

QnAbstractStreamDataProvider* HanwhaResource::createLiveDataProvider()
{
    return new HanwhaStreamReader(toSharedPointer(this));
}

nx::core::resource::AbstractRemoteArchiveManager* HanwhaResource::remoteArchiveManager()
{
    if (!m_remoteArchiveManager)
        m_remoteArchiveManager = std::make_unique<HanwhaRemoteArchiveManager>(this);

    return m_remoteArchiveManager.get();
}

QnCameraAdvancedParamValueMap HanwhaResource::getApiParameters(const QSet<QString>& ids)
{
    if (ids.isEmpty())
        return QnCameraAdvancedParamValueMap();

    QnCameraAdvancedParamValueMap result;

    using ParameterList = std::vector<QString>;
    using SubmenuMap = std::map<QString, ParameterList>;
    using CgiMap = std::map<QString, SubmenuMap>;

    CgiMap requests;
    std::multimap<QString, QString> parameterNameToId;

    for (const auto& id: ids)
    {
        const auto parameter = m_advancedParametersProvider.getParameterById(id);
        if (parameter.dataType == QnCameraAdvancedParameter::DataType::Button)
            continue;

        const auto info = advancedParameterInfo(id);
        if (!info || !info->isValid())
            continue;

        const bool isProfileDependent = info->profileDependency() != Qn::CR_Default;
        if (isProfileDependent && isCameraControlDisabled())
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
            if (commonModule()->isNeedToStop())
                return QnCameraAdvancedParamValueMap();

            auto submenu = submenuEntry.first;
            HanwhaRequestHelper helper(sharedContext(), bypassChannel());
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
                    const auto parameter = m_advancedParametersProvider.getParameterById(parameterId);
                    const auto info = advancedParameterInfo(parameterId);
                    if (!info)
                        continue;

                    if (info->cgi() != cgi || info->submenu() != submenu)
                        continue;

                    if (!info->isChannelIndependent())
                    {
                        // TODO: #dmishin most likely here will be issues with multisensor cameras
                        // (we will always read from the first channel only). Fix it.
                        parameterString += kHanwhaChannelPropertyTemplate.arg(
                            isBypassSupported() ? 0 : getChannel());
                    }

                    auto profile = profileByRole(info->profileDependency(), isBypassSupported());
                    if (profile != kHanwhaInvalidProfile)
                        parameterString += lit(".Profile.%1").arg(profile);

                    const auto fullParameterNameTemplate = lit("%1.%2")
                        .arg(parameterString);

                    auto fullParameterName = fullParameterNameTemplate.arg(parameterName);
                    auto value = response.parameter<QString>(fullParameterName);

                    if (!value)
                    {
                        // Some cameras sometimes send parameter name in wrong register.
                        fullParameterName = fullParameterNameTemplate.arg(
                            parameterName.toLower());
                        value = response.parameter<QString>(fullParameterName);
                    }

                    if (!value)
                        value = defaultValue(parameterName, info->profileDependency());

                    if (!value)
                        continue;

                    result.insert(
                        parameterId,
                        fromHanwhaAdvancedParameterValue(parameter, *info, *value));
                }
            }
        }
    }

    return result;
}

QSet<QString> HanwhaResource::setApiParameters(const QnCameraAdvancedParamValueMap& values)
{
    using ParameterMap = std::map<QString, QString>;

    bool reopenPrimaryStream = false;
    bool reopenSecondaryStream = false;

    std::map<UpdateInfo, ParameterMap> requests;

    const auto buttonParameter = findButtonParameter(values.toValueList());
    if (buttonParameter)
    {
        const auto buttonInfo = advancedParameterInfo(buttonParameter->id);
        if (!buttonInfo)
            return {};

        const bool success = executeCommand(*buttonParameter);
        const auto streamsToReopen = buttonInfo->streamsToReopen();
        reopenStreams(
            streamsToReopen.contains(Qn::ConnectionRole::CR_LiveVideo),
            streamsToReopen.contains(Qn::ConnectionRole::CR_SecondaryLiveVideo));

        return success ? QSet<QString>{buttonParameter->id} : QSet<QString>();
    }

    const auto filteredParameters =
        addAssociatedParameters(filterGroupParameters(values.toValueList()));

    for (const auto& value: filteredParameters)
    {
        UpdateInfo updateInfo;
        const auto info = advancedParameterInfo(value.id);
        if (!info)
            continue;

        const auto parameter = m_advancedParametersProvider.getParameterById(value.id);
        const auto resourceProperty = info->resourceProperty();

        if (!resourceProperty.isEmpty())
            setProperty(resourceProperty, value.value);

        updateInfo.cgi = info->cgi();
        updateInfo.submenu = info->submenu();
        updateInfo.action = info->updateAction();
        updateInfo.channelIndependent = info->isChannelIndependent();
        updateInfo.profile = profileByRole(info->profileDependency(), isBypassSupported());

        const auto streamsToReopen = info->streamsToReopen();

        if (updateInfo.profile == profileByRole(Qn::ConnectionRole::CR_LiveVideo)
            || streamsToReopen.contains(Qn::ConnectionRole::CR_LiveVideo))
        {
            reopenPrimaryStream = true;
        }

        if (updateInfo.profile == profileByRole(Qn::ConnectionRole::CR_SecondaryLiveVideo)
            || streamsToReopen.contains(Qn::ConnectionRole::CR_SecondaryLiveVideo))
        {
            reopenSecondaryStream = true;
        }

        requests[updateInfo][info->parameterName()] = toHanwhaAdvancedParameterValue(
            parameter,
            *info,
            value.value);
    }
    saveProperties();

    bool success = true;
    for (const auto& request: requests)
    {
        if (commonModule()->isNeedToStop())
            return QSet<QString>();

        const auto requestCommon = request.first;
        auto requestParameters = request.second;

        if (!requestCommon.channelIndependent)
            requestParameters[kHanwhaChannelProperty] = QString::number(getChannel());

        if (requestCommon.profile != kHanwhaInvalidProfile)
        {
            requestParameters[kHanwhaProfileNumberProperty]
                = QString::number(requestCommon.profile);
        }

        HanwhaRequestHelper helper(sharedContext(), bypassChannel());
        const auto response = helper.doRequest(
            requestCommon.cgi,
            requestCommon.submenu,
            requestCommon.action,
            nx::utils::RwLockType::write,
            requestParameters);

        if (!response.isSuccessful())
        {
            success = false;
            continue;
        }
    }

    if (reopenPrimaryStream || reopenSecondaryStream)
    {
        initMediaStreamCapabilities();
        saveProperties();
        reopenStreams(reopenPrimaryStream, reopenSecondaryStream);
    }

    return success ? values.ids() : QSet<QString>();
}

bool HanwhaResource::setOutputPortState(
    const QString& outputId,
    bool activate,
    unsigned int autoResetTimeoutMs)
{
    auto resetHandler =
        [state = !activate, outputId, this]()
        {
            setOutputPortStateInternal(outputId, state);
        };

    if (autoResetTimeoutMs > 0)
    {
        m_timerHolder.addTimer(
            lm("%1 output %2").args(this, outputId),
            resetHandler,
            std::chrono::milliseconds(autoResetTimeoutMs));
    }

    return setOutputPortStateInternal(outputId, activate);
}

void HanwhaResource::startInputPortStatesMonitoring()
{
    m_areInputPortsMonitored = true;
}

void HanwhaResource::stopInputPortStatesMonitoring()
{
    m_areInputPortsMonitored = false;
}

bool HanwhaResource::captureEvent(const nx::vms::event::AbstractEventPtr& event)
{
    if (!m_areInputPortsMonitored)
        return false;

    const auto analyticsEvent = event.dynamicCast<nx::vms::event::AnalyticsSdkEvent>();
    if (!analyticsEvent)
        return false;

    const auto parameters = analyticsEvent->getRuntimeParams();
    if (parameters.getAnalyticsEventTypeId() != kHanwhaInputPortEventTypeId)
        return false;

    emit inputPortStateChanged(
        toSharedPointer(this),
        analyticsEvent->auxiliaryData(),
        analyticsEvent->getToggleState() == nx::vms::api::EventState::active,
        parameters.eventTimestampUsec);

    return true;
}

bool HanwhaResource::isAnalyticsDriverEvent(nx::vms::api::EventType eventType) const
{
    return base_type::isAnalyticsDriverEvent(eventType)
        || eventType == nx::vms::api::EventType::cameraInputEvent;
}

SessionContextPtr HanwhaResource::session(
    HanwhaSessionType sessionType,
    const QnUuid& clientId,
    bool generateNewOne)
{
    if (const auto context = sharedContext())
        return m_sharedContext->session(sessionType, clientId, generateNewOne);

    return SessionContextPtr();
}

std::unique_ptr<QnAbstractArchiveDelegate> HanwhaResource::remoteArchiveDelegate()
{
    return std::make_unique<HanwhaArchiveDelegate>(toSharedPointer(this));
}

bool HanwhaResource::isVideoSourceActive()
{
    auto videoSources = sharedContext()->videoSources();
    auto eventStatuses = sharedContext()->eventStatuses();
    if (!videoSources || !eventStatuses)
        return false;

    const auto state = videoSources->parameter<QString>(
        lit("Channel.%1.State").arg(getChannel()));

    if (!state.is_initialized())
        return false;

    const auto videoLossParameter = eventStatuses->parameter<bool>(
        lit("Channel.%1.Videoloss").arg(getChannel()));

    const bool videoLoss = videoLossParameter.is_initialized()
        ? *videoLossParameter
        : false;

    return state == kHanwhaVideoSourceStateOn && !videoLoss;
}

int HanwhaResource::maxProfileCount() const
{
    return m_maxProfileCount;
}

nx::vms::server::resource::StreamCapabilityMap HanwhaResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex /*streamIndex*/)
{
    // TODO: implement me
    return nx::vms::server::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result HanwhaResource::initializeCameraDriver()
{
    setCameraCapability(Qn::customMediaPortCapability, true);
    const auto result = initDevice();
    if (!result)
    {
        const auto status = result.errorCode == CameraDiagnostics::ErrorCode::notAuthorised
            ? Qn::Unauthorized
            : Qn::Offline;

        setStatus(status);
    }

    return result;
}

CameraDiagnostics::Result HanwhaResource::initDevice()
{
    setCameraCapability(Qn::SetUserPasswordCapability, true);
    bool isDefaultPassword = false;
    bool isOldFirmware = false;
    auto isDefaultPasswordGuard = nx::utils::makeScopeGuard(
        [&]
        {
            setCameraCapability(Qn::IsDefaultPasswordCapability, isDefaultPassword);
            setCameraCapability(Qn::IsOldFirmwareCapability, isOldFirmware);
            saveProperties();
        });

    const auto sharedContext = serverModule()->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(toSharedPointer(this));

    {
        QnMutexLocker lock(&m_mutex);
        m_sharedContext = sharedContext;
    }

    m_sharedContext->setResourceAccess(getUrl(), getAuth());
    m_sharedContext->setChunkLoaderSettings(
        {
            std::chrono::seconds(qnGlobalSettings->hanwhaChunkReaderResponseTimeoutSeconds()),
            std::chrono::seconds(qnGlobalSettings->hanwhaChunkReaderMessageBodyTimeoutSeconds())
        });

    const auto info = m_sharedContext->information();
    if (!info)
        return info.diagnostics;

    CameraDiagnostics::Result result = initSystem(info.value);
    if (!result)
        return result;

    if (isNvr())
        sharedContext->startServices();

    auto resData = resourceData();
    auto minFirmwareVersion = resData.value<QString>(lit("minimalFirmwareVersion"));
    if (!minFirmwareVersion.isEmpty() &&
        !getFirmware().isEmpty()
        && minFirmwareVersion > getFirmware())
    {
        isOldFirmware = true;
        return CameraDiagnostics::CameraOldFirmware(
            minFirmwareVersion, getFirmware());
    }

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

    result = initRemoteArchive();
    if (!result)
        return result;

    result = initAdvancedParameters();
    if (!result)
        return result;

    if (!isNvr() && hasCameraCapabilities(Qn::RemoteArchiveCapability))
        sharedContext->startServices();

    // it's saved in isDefaultPasswordGuard
    isDefaultPassword = getAuth() == HanwhaResourceSearcher::getDefaultAuth();

    return result;
}

void HanwhaResource::initMediaStreamCapabilities()
{
    m_capabilities.streamCapabilities[Qn::StreamIndex::primary] =
        mediaCapabilityForRole(Qn::ConnectionRole::CR_LiveVideo);
    m_capabilities.streamCapabilities[Qn::StreamIndex::secondary] =
        mediaCapabilityForRole(Qn::ConnectionRole::CR_SecondaryLiveVideo);
    setProperty(
        ResourcePropertyKey::kMediaCapabilities,
        QString::fromLatin1(QJson::serialized(m_capabilities)));
}

nx::media::CameraStreamCapability HanwhaResource::mediaCapabilityForRole(Qn::ConnectionRole role) const
{
    nx::media::CameraStreamCapability capability;
    if (!isConnectedViaSunapi())
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

    if (!limits)
        return capability;

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

QnAbstractPtzController* HanwhaResource::createPtzControllerInternal() const
{
    auto controller = new HanwhaPtzController(toSharedPointer(this));
    controller->setPtzCapabilities(m_ptzCapabilities);
    controller->setPtzLimits(m_ptzLimits);
    controller->setPtzTraits(m_ptzTraits);
    controller->setPtzRanges(m_ptzRanges);

    return controller;
}

CameraDiagnostics::Result HanwhaResource::initSystem(const HanwhaInformation& info)
{
    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    m_deviceType = info.deviceType;
    const auto nxDeviceType = fromHanwhaToNxDeviceType(deviceType());

    // Set device type only for NVRs and encoders due to optimization purposes.
    if (nx::core::resource::isProxyDeviceType(nxDeviceType))
        setDeviceType(nxDeviceType);

    if (!info.firmware.isEmpty())
        setFirmware(info.firmware);

    m_attributes = std::move(info.attributes);
    m_cgiParameters = std::move(info.cgiParameters);
    m_isChannelConnectedViaSunapi = true;

    if (isNvr())
    {
        // We always try to pull an audio stream if possible for NVRs.
        setProperty(ResourcePropertyKey::kForcedAudioStream, 1);

        setCameraCapability(Qn::IsPlaybackSpeedSupported, true);
        setCameraCapability(Qn::DeviceBasedSync, true);
        setCameraCapability(Qn::DualStreamingForLiveOnly, true);

        const auto sunapiSupportAttribute = m_attributes.attribute<bool>(
            lit("Media/Protocol.SUNAPI/%1").arg(getChannel()));

        m_isChannelConnectedViaSunapi = sunapiSupportAttribute != boost::none
            && sunapiSupportAttribute.get();

        initBypass();

        if (isBypassSupported())
        {
            HanwhaRequestHelper helper(sharedContext(), /*bypassChannel*/ getChannel());
            m_bypassDeviceAttributes = helper.fetchAttributes(lit("attributes"));
            m_bypassDeviceCgiParameters = helper.fetchCgiParameters(lit("cgis"));

            if (!m_bypassDeviceAttributes.isValid())
            {
                return CameraDiagnostics::CameraInvalidParams(
                    lit("Can't fetch proxied device attributes"));
            }

            if (!m_bypassDeviceCgiParameters.isValid())
            {
                return CameraDiagnostics::CameraInvalidParams(
                    lit("Can't fetch proxied device CGI parameters"));
            }

            const auto proxiedChannelCount = m_bypassDeviceAttributes
                .attribute<int>(lit("System/MaxChannel"));

            if (proxiedChannelCount == boost::none)
            {
                return CameraDiagnostics::CameraInvalidParams(
                    lit("Can't fetch proxied channel count"));
            }

            m_proxiedDeviceChannelCount = proxiedChannelCount.get();

            const auto proxiedDeviceInfo = helper.view(lit("system/deviceinfo"));
            handleProxiedDeviceInfo(proxiedDeviceInfo);
        }
    }

    const auto hasRs485 = attributes().attribute<bool>("IO/RS485");
    const auto hasRs422 = attributes().attribute<bool>("IO/RS422");

    m_hasSerialPort = (hasRs485 != boost::none && *hasRs485)
        || (hasRs422 != boost::none && *hasRs422);

    if (isAnalogEncoder() || isProxiedAnalogEncoder() || hasSerialPort())
    {
        // We can't reliably determine if there's PTZ caps for analogous cameras
        // connected to Hanwha encoder, so we allow a user to enable it on the 'expert' tab
        setIsUserAllowedToModifyPtzCapabilities(true);
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initBypass()
{
    if (ini().disableBypass)
    {
        NX_WARNING(this, lit("Bypass is disabled via hanwha.ini config"));
        m_isBypassSupported = false;
        return CameraDiagnostics::NoErrorResult();
    }

    const auto resData = resourceData();
    const auto bypassOverride = resData.value<resource_data_support::HanwhaBypassSupportType>(
        kHanwhaBypassOverrideParameterName,
        resource_data_support::HanwhaBypassSupportType::normal);

    if (bypassOverride == resource_data_support::HanwhaBypassSupportType::forced)
    {
        m_isBypassSupported = true;
        return CameraDiagnostics::NoErrorResult();
    }
    else if (bypassOverride == resource_data_support::HanwhaBypassSupportType::disabled)
    {
        m_isBypassSupported = false;
        return CameraDiagnostics::NoErrorResult();
    }

    const auto bypassSupportResult = sharedContext()->isBypassSupported();
    if (!bypassSupportResult)
        return bypassSupportResult.diagnostics;

    const HanwhaFirmware firmware(getFirmware());
    auto firmwareRequiredForBypass = kHanwhaDefaultMinimalBypassFirmware;
    if (resData.contains(kHanwhaMinimalBypassFirmwareParameterName))
    {
        firmwareRequiredForBypass = resData.value<QString>(
            kHanwhaMinimalBypassFirmwareParameterName);
    }

    m_isBypassSupported = bypassSupportResult.value
        && firmware >= HanwhaFirmware(firmwareRequiredForBypass);

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initMedia()
{
    if (!ini().initMedia)
        return CameraDiagnostics::NoErrorResult();

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    if (isNvr() && !isVideoSourceActive())
        return CameraDiagnostics::CameraInvalidParams("Video source is not active");

    const auto videoProfiles = sharedContext()->videoProfiles();
    if (!videoProfiles)
        return videoProfiles.diagnostics;

    const auto channel = getChannel();
    m_capabilities.hasAudio = isNvr() //< We assume NVR channels are always capable of audio stream.
        || m_attributes.attribute<int>(lm("Media/MaxAudioInput/%1").arg(channel)) > 0;

    if (isConnectedViaSunapi())
    {
        setProfileSessionPolicy();

        int fixedProfileCount = 0;
        const auto channelPrefix = kHanwhaChannelPropertyTemplate.arg(channel);
        for (const auto& entry: videoProfiles->response())
        {
            const auto propertyChannel = extractPropertyChannel(entry.first);
            if (propertyChannel == boost::none || *propertyChannel != channel)
                continue;

            const bool isFixedProfile = entry.first.endsWith(kHanwhaIsFixedProfileProperty)
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
        m_capabilities.hasDualStreaming = m_maxProfileCount - fixedProfileCount > 1;

        auto result = fetchCodecInfo(&m_codecInfo);
        if (!result)
            return result;

        initMediaStreamCapabilities();

        result = createNxProfiles();
        if (!result)
            return result;

        if (isNvr())
        {
            result = setUpProfilePolicies(
                profileByRole(Qn::ConnectionRole::CR_LiveVideo),
                profileByRole(Qn::ConnectionRole::CR_SecondaryLiveVideo));

            if (!result)
                return result;
        }
    }
    else
    {
        fetchExistingProfiles();
        m_capabilities.hasDualStreaming
            = (profileByRole(Qn::CR_SecondaryLiveVideo) != kHanwhaInvalidProfile);

        initMediaStreamCapabilities();
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::setProfileSessionPolicy()
{
    const auto sessionPolicyParameter = m_cgiParameters
        .parameter(lit("network/rtsp/set/ProfileSessionPolicy"));

    if (!sessionPolicyParameter)
    {
        NX_VERBOSE(
            this,
            lm("ProfileSessionPolicy parameter is not available for %1 (%2)")
                .args(getName(), getUniqueId()));

        return CameraDiagnostics::NoErrorResult();
    }

    HanwhaRequestHelper helper(sharedContext());
    auto result = helper.set(
        lit("network/rtsp"),
        {{lit("ProfileSessionPolicy"), lit("Disconnect")}});

    if (!result.isSuccessful())
    {
        NX_WARNING(
            this,
            lm("Failed to appliy 'Disconnect' rtsp profile session policy to %1 (%2)")
                .args(getName(), getUniqueId()));

        // Ignore this error since it's not critical.
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initIo()
{
    if (!ini().initIo)
        return CameraDiagnostics::NoErrorResult();

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    QnIOPortDataList ioPorts;

    const auto maxAlarmInputs = m_attributes.attribute<int>("Eventsource/MaxAlarmInput");
    const auto maxAlarmOutputs = m_attributes.attribute<int>("IO/MaxAlarmOutput");
    const auto maxAuxDevices = m_attributes.attribute<int>("IO/MaxAux");

    if (isNvr())
    {
        const auto networkAlarmInputChannels =
            m_cgiParameters.parameter("eventsources/networkalarminput/view/Channel");

        const bool gotNetworkAlarmInput = networkAlarmInputChannels.is_initialized()
            && networkAlarmInputChannels->possibleValues()
                .contains(QString::number(getChannel()));

        if (gotNetworkAlarmInput)
        {
            QnIOPortData inputPortData;
            inputPortData.portType = Qn::PT_Input;
            inputPortData.id = lm("Channel.%1.NetworkAlarmInput").args(getChannel());
            inputPortData.inputName = lm("Device Alarm Input #1");

            m_ioPortTypeById[inputPortData.id] = inputPortData;
            ioPorts.push_back(inputPortData);

            HanwhaRequestHelper helper(sharedContext());
            helper.set(
                "eventsources/networkalarminput",
                {
                    {kHanwhaChannelProperty, QString::number(getChannel())},
                    {"Enable", kHanwhaTrue}
                });
        }

        if (isBypassSupported())
        {
            const auto proxiedOutputCount =
                m_bypassDeviceAttributes.attribute<int>("IO/MaxAlarmOutput");

            if (proxiedOutputCount.is_initialized() && *proxiedOutputCount > 0)
            {
                for (auto i = 1; i <= proxiedOutputCount.get(); ++i)
                {
                    QnIOPortData outputPortData;
                    outputPortData.portType = Qn::PT_Output;
                    outputPortData.id = lm("%1.AlarmOutput.%2").args(kBypassPrefix, i);
                    outputPortData.outputName = lm("Device Alarm Output #%1").args(i);

                    m_ioPortTypeById[outputPortData.id] = outputPortData;
                    ioPorts.push_back(outputPortData);
                }
            }
        }
    }

    if (maxAlarmInputs.is_initialized() && *maxAlarmInputs > 0)
    {
        for (auto i = 1; i <= maxAlarmInputs.get(); ++i)
        {
            QnIOPortData inputPortData;
            inputPortData.portType = Qn::PT_Input;
            inputPortData.id = lit("AlarmInput.%1").arg(i);
            inputPortData.inputName = tr("%1Alarm Input #%2")
                .arg(isNvr() ? lit("NVR ") : QString())
                .arg(i);

            m_ioPortTypeById[inputPortData.id] = inputPortData;
            ioPorts.push_back(inputPortData);
        }

        HanwhaRequestHelper helper(sharedContext());
        helper.set(
            lit("eventsources/alarminput"),
            {{lit("AlarmInput.%1.Enable").arg(getChannel()), kHanwhaTrue}});
    }

    if (maxAlarmOutputs.is_initialized() && *maxAlarmOutputs > 0)
    {
        for (auto i = 1; i <= maxAlarmOutputs.get(); ++i)
        {
            QnIOPortData outputPortData;
            outputPortData.portType = Qn::PT_Output;
            outputPortData.id = lm("AlarmOutput.%1").args(i);
            outputPortData.outputName = lm("%1Alarm Output #%2")
                .args((isNvr() ? lit("NVR ") : QString()), i);

            m_ioPortTypeById[outputPortData.id] = outputPortData;
            ioPorts.push_back(outputPortData);

            if (m_defaultOutputPortId.isEmpty())
                m_defaultOutputPortId = outputPortData.id;
        }
    }

    if (maxAuxDevices.is_initialized() && *maxAuxDevices > 0)
    {
        for (auto i = 1; i <= maxAuxDevices.get(); ++i)
        {
            QnIOPortData outputPortData;
            outputPortData.portType = Qn::PT_Output;
            outputPortData.id = lm("Aux.%1").args(i);
            outputPortData.outputName = lm("Auxiliary Device #%1").args(i);

            m_ioPortTypeById[outputPortData.id] = outputPortData;
            ioPorts.push_back(outputPortData);
        }
    }

    setIoPortDescriptions(std::move(ioPorts), /*needMerge*/ true);
    return CameraDiagnostics::NoErrorResult();
}

static QString ptzCapabilityBits(Ptz::Capabilities capabilities)
{
    return QString::number(static_cast<int>(capabilities), 2);
}

CameraDiagnostics::Result HanwhaResource::initPtz()
{
    if (!ini().initPtz)
        return CameraDiagnostics::NoErrorResult();

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    setUserPreferredPtzPresetType(nx::core::ptz::PresetType::native);

    const auto mainDescriptors = isNvr()
        ? kHanwhaNvrPtzCapabilityDescriptors
        : kHanwhaCameraPtzCapabilityDescriptors;

    auto& capabilities = m_ptzCapabilities[core::ptz::Type::operational];
    capabilities = calculateSupportedPtzCapabilities(
        mainDescriptors,
        m_attributes,
        getChannel());

    NX_VERBOSE(this, "Supported PTZ capabilities direct: %1", ptzCapabilityBits(capabilities));
    if (isBypassSupported())
    {
        const auto bypassPtzCapabilities = calculateSupportedPtzCapabilities(
            kHanwhaCameraPtzCapabilityDescriptors,
            m_bypassDeviceAttributes,
            0); //< TODO: #dmishin is it correct for multichannel resources connected to a NVR?

        NX_VERBOSE(this, "Supported PTZ capabilities bypass: %1",
            ptzCapabilityBits(bypassPtzCapabilities));

        // We consider capability is true if it's supported both by a NVR and a camera.
        capabilities &= bypassPtzCapabilities;
        NX_VERBOSE(this, "Supported PTZ capabilities both: %1", ptzCapabilityBits(capabilities));
    }

    if ((capabilities & Ptz::AbsolutePtzCapabilities) == Ptz::AbsolutePtzCapabilities)
        capabilities |= Ptz::DevicePositioningPtzCapability;

    initConfigurationalPtz();

    m_ptzRanges = fetchPtzRanges();
    m_ptzLimits = calculatePtzLimits(m_attributes, m_cgiParameters, getChannel());
    m_ptzTraits.append(calculatePtzTraits());

    if (m_ptzTraits.contains(Ptz::ManualAutoFocusPtzTrait))
        capabilities |= Ptz::AuxiliaryPtzCapability;

    initRedirectedAreaZoomPtz();

    NX_DEBUG(this, "Supported PTZ capabilities: %1", ptzCapabilityBits(capabilities));
    if (isAnalogEncoder())
    {
        // Encoder PTZ capabilities are being overriden from 'Expert' tab
        // and are empty by default.
        m_ptzCapabilities[core::ptz::Type::operational] = Ptz::NoPtzCapabilities;
        m_ptzCapabilities[core::ptz::Type::configurational] = Ptz::NoPtzCapabilities;
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initConfigurationalPtz()
{
    const auto channel = getChannel();
    auto& configurationalCapabilities = m_ptzCapabilities[core::ptz::Type::configurational];
    if (ini().forceLensControl)
    {
        configurationalCapabilities = Ptz::ContinuousPtzCapabilities
            | Ptz::ContinuousRotationCapability
            | Ptz::ContinuousFocusCapability;

        return CameraDiagnostics::NoErrorResult();
    }

    for (const auto& descriptor: kHanwhaConfigurationalPtzDescriptors)
    {
        bool hasCapability = true;
        if (!descriptor.supportAttribute.isEmpty())
        {
            const auto attribute = attributes()
                .attribute<bool>(lm("%1/%2").args(
                    descriptor.supportAttribute,
                    (isBypassSupported() ? 0 : channel)));

            hasCapability = attribute != boost::none && *attribute;
        }

        if (!hasCapability)
            continue;

        const auto parameters = cgiParameters();
        const auto parameter = cgiParameters().parameter(descriptor.valueParameter);
        if (parameter == boost::none || !parameter->isValid())
            continue;

        if (qnGlobalSettings->showHanwhaAlternativePtzControlsOnTile())
            m_ptzCapabilities[core::ptz::Type::operational] |= descriptor.capabilities;

        configurationalCapabilities |= descriptor.capabilities;
        m_ptzRanges[core::ptz::Type::configurational][parameter->name()] = HanwhaRange(*parameter);
    }

    NX_VERBOSE(this, lm("%1: Supported PTZ capabilities alternative: %2")
        .args(getPhysicalId(), ptzCapabilityBits(configurationalCapabilities)));
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initRedirectedAreaZoomPtz()
{
    const auto ptzTargetChannel = resourceData().value<int>(ResourceDataKey::kPtzTargetChannel, -1);
    if (ptzTargetChannel == -1 || ptzTargetChannel == getChannel())
        return CameraDiagnostics::NoErrorResult();

    const auto calibratedChannels = sharedContext()->ptzCalibratedChannels();
    const auto isCalibrated = calibratedChannels && calibratedChannels->count(getChannel());
    if (isCalibrated)
    {
        const auto id = physicalIdForChannel(getGroupId(), ptzTargetChannel);
        NX_DEBUG(this, "Set PTZ target id: %1", id);
        setProperty(ResourcePropertyKey::kPtzTargetId, id);

        // TODO: Remove this workaround when client fixes a bug:
        //     Absolute move and viewport is not accessible if continious move is not supportd.
        //
        // m_ptzCapabilities[core::ptz::Type::operational] |= Ptz::ContinuousPanTiltCapabilities;
    }
    else
    {
        NX_DEBUG(this, "Remove PTZ target id");
        setProperty(ResourcePropertyKey::kPtzTargetId, QString());
        m_ptzCapabilities[core::ptz::Type::operational] = Ptz::NoPtzCapabilities;
    }

    setPtzCalibarionTimer();
    return calibratedChannels.diagnostics;
}

HanwhaPtzRangeMap HanwhaResource::fetchPtzRanges()
{
    HanwhaPtzRangeMap result;

    auto isParameterOk =
        [](const auto& parameter)
        {
            if (parameter == boost::none)
                return false;

            const auto parameterType = parameter->type();
            if (parameterType == HanwhaCgiParameterType::enumeration)
                return true;

            const double min = parameterType == HanwhaCgiParameterType::floating
                ? parameter->floatMin()
                : (float) parameter->min();

            const double max = parameterType == HanwhaCgiParameterType::floating
                ? parameter->floatMax()
                : (float) parameter->max();

            return !qFuzzyEquals(min, max) && max > min;
        };

    for (const auto& descriptor: kRangeDescriptors)
    {
        const auto& parameters = descriptor.ptzTypes.testFlag(nx::core::ptz::Type::configurational)
            ? cgiParameters() //< We can use bypass for configurational ptz.
            : m_cgiParameters;

        if (descriptor.cgiParameter.isEmpty())
        {
            NX_ASSERT(false, "Descriptor should have main CGI parameter.");
            continue;
        }
        auto parameter = parameters.parameter(descriptor.cgiParameter);
        if (!isParameterOk(parameter))
        {
            if (descriptor.alternativeCgiParameter.isEmpty())
                continue;

            const auto alternativeParameter = parameters.parameter(
                descriptor.alternativeCgiParameter);

            if (!isParameterOk(alternativeParameter))
                continue;

            const auto range = alternativeParameter->floatMax() - alternativeParameter->floatMin();
            parameter->setFloatRange({-range, range});
        }

        HanwhaRange range(*parameter);
        for (auto ptzType: {core::ptz::Type::operational, core::ptz::Type::configurational})
        {
            if (descriptor.ptzTypes.testFlag(ptzType))
                result[ptzType][descriptor.name] = range;
        }
    }

    return result;
}

QnPtzAuxiliaryTraitList HanwhaResource::calculatePtzTraits() const
{
    QnPtzAuxiliaryTraitList ptzTraits;
    if (deviceType() == HanwhaDeviceType::nwc) //< Camera device type.
        ptzTraits = calculateCameraOnlyTraits();

    calculateAutoFocusSupport(&ptzTraits);
    return ptzTraits;
}

QnPtzAuxiliaryTraitList HanwhaResource::calculateCameraOnlyTraits() const
{
    QnPtzAuxiliaryTraitList ptzTraits;
    for (const auto& item: kHanwhaPtzTraitDescriptors)
    {
        const auto trait = QnPtzAuxiliaryTrait(item.first);
        const auto& descriptor = item.second;
        const auto& parameter = m_cgiParameters.parameter(descriptor.parameterName);

        if (parameter == boost::none)
            continue;

        for (const auto& value: descriptor.positiveValues)
        {
            if (parameter->isValueSupported(value))
            {
                ptzTraits.push_back(trait);
                break;
            }
        }
    }

    return ptzTraits;
}

void HanwhaResource::calculateAutoFocusSupport(QnPtzAuxiliaryTraitList* outTraitList) const
{
    const auto parameter = cgiParameters().parameter(lit("image/focus/control/Mode"));
    if (!parameter)
        return;

    bool gotAutoFocus = false;
    const auto possibleValues = parameter->possibleValues();

    if (!isNvr() || isBypassSupported())
    {
        static const std::map<QString, QString> kAutoFocusModes = {
            {kHanwhaSimpleFocusTrait, lit("SimpleFocus")},
            {kHanwhaAutoFocusTrait, lit("AutoFocus")}
        };

        for (const auto& entry: kAutoFocusModes)
        {
            const auto& traitName = entry.first;
            const auto& mode = entry.second;

            if (possibleValues.contains(mode))
            {
                outTraitList->push_back(QnPtzAuxiliaryTrait(traitName));
                gotAutoFocus = true;
            }
        }
    }
    else
    {
        // NVR without bypass.
        const auto attribute = attributes().attribute<bool>(
            lm("Image/%1/SimpleFocus").arg(getChannel()));

        if (attribute != boost::none && attribute.get())
        {
            outTraitList->push_back(QnPtzAuxiliaryTrait(kHanwhaSimpleFocusTrait));
            gotAutoFocus = true;
        }
    }

    if (gotAutoFocus)
        outTraitList->push_back(QnPtzAuxiliaryTrait(Ptz::ManualAutoFocusPtzTrait));
}

CameraDiagnostics::Result HanwhaResource::initAdvancedParameters()
{
    if (!ini().initAdvancedParameters)
        return CameraDiagnostics::NoErrorResult();

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    if (isNvr() && !isBypassSupported())
    {
        m_advancedParametersProvider.assign(QnCameraAdvancedParams());
        return CameraDiagnostics::NoErrorResult();
    }

    QnCameraAdvancedParams parameters;
    QFile advancedParametersFile(kAdvancedParametersTemplateFile);

    bool result = QnCameraAdvacedParamsXmlParser::readXml(&advancedParametersFile, parameters);
    NX_ASSERT(result, lm("Error while parsing xml: %1").arg(kAdvancedParametersTemplateFile));
    if (!result)
        return CameraDiagnostics::NoErrorResult();

    const auto allowedParameters = QString(ini().enabledAdvancedParameters)
        .split(L',', QString::SplitBehavior::SkipEmptyParts);
    for (const auto& id: parameters.allParameterIds())
    {
        if (!allowedParameters.isEmpty() && !allowedParameters.contains(id))
            continue;

        const auto parameter = parameters.getParameterById(id);
        HanwhaAdavancedParameterInfo info(parameter);

        if(!info.isValid())
            continue;

        if (!info.isDeviceTypeSupported(deviceType()))
            continue;

        m_advancedParameterInfos.emplace(id, info);
    }

    bool success = fillRanges(&parameters, cgiParameters());
    if (!success)
        return CameraDiagnostics::NoErrorResult();

    m_advancedParametersProvider.assign(filterParameters(parameters));
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initTwoWayAudio()
{
    if (!ini().initTwoWayAudio)
        return CameraDiagnostics::NoErrorResult();

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    HanwhaRequestHelper helper(sharedContext());
    auto response = helper.view(
        lit("media/audiooutput"),
        {{kHanwhaChannelProperty, QString::number(getChannel())}});

    const auto isAudioOutputEnabled = response.parameter<bool>(lit("Enable"), getChannel());
    if (!isAudioOutputEnabled || !isAudioOutputEnabled.get())
        return CameraDiagnostics::NoErrorResult();

    const auto bitrateParam = response.parameter<int>(lit("Bitrate"), getChannel());
    int bitrateKbps = bitrateParam ? *bitrateParam : 0;

    m_audioTransmitter.reset(new OnvifAudioTransmitter(this));
    if (bitrateKbps > 0)
        m_audioTransmitter->setBitrateKbps(bitrateKbps);

    setCameraCapability(Qn::AudioTransmitCapability, true);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::initRemoteArchive()
{
    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    if (!ini().enableEdge || isNvr())
    {
        setCameraCapability(Qn::RemoteArchiveCapability, false);
        return CameraDiagnostics::NoErrorResult();
    }

    HanwhaRequestHelper helper(sharedContext());
    auto response = helper.view(lit("recording/storage"));
    if (!response.isSuccessful())
    {
        setCameraCapability(Qn::RemoteArchiveCapability, false);
        return CameraDiagnostics::NoErrorResult();
    }

    const auto storageEnabled = response.parameter<bool>(lit("Enable"));
    const bool hasRemoteArchive = (storageEnabled != boost::none) && *storageEnabled;
    setCameraCapability(Qn::RemoteArchiveCapability, hasRemoteArchive);

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::handleProxiedDeviceInfo(
    const HanwhaResponse& deviceInfoResponse)
{
    if (deviceInfoResponse.isSuccessful())
    {
        const auto deviceInfoParameter = deviceInfoResponse.parameter<QString>("DeviceType");
        m_bypassDeviceType = deviceInfoParameter
            ? QnLexical::deserialized<HanwhaDeviceType>(
                deviceInfoParameter->trimmed(),
                HanwhaDeviceType::unknown)
            : HanwhaDeviceType::unknown;

        const auto proxiedIdParameter =
            deviceInfoResponse.parameter<QString>("ConnectedMACAddress");

        if (proxiedIdParameter == boost::none)
            return CameraDiagnostics::NoErrorResult();

        const auto proxiedDeviceId = proxiedIdParameter->trimmed();
        if (proxiedDeviceId != proxiedId())
        {
            cleanUpOnProxiedDeviceChange();
            setProxiedId(proxiedDeviceId);
        }
    }
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::createNxProfiles()
{
    std::map <Qn::ConnectionRole, boost::optional<HanwhaVideoProfile>> profilesByRole = {
        {Qn::ConnectionRole::CR_LiveVideo, boost::none},
        {Qn::ConnectionRole::CR_SecondaryLiveVideo, boost::none}
    };

    std::set<HanwhaProfileNumber> profilesToRemove;
    int totalProfileNumber = 0;

    m_profileByRole.clear();

    auto result = findProfiles(
        &profilesByRole[Qn::ConnectionRole::CR_LiveVideo],
        &profilesByRole[Qn::ConnectionRole::CR_SecondaryLiveVideo],
        &totalProfileNumber,
        &profilesToRemove);

    if (!result)
        return result;

    if (!qnGlobalSettings->hanwhaDeleteProfilesOnInitIfNeeded())
    {
        int amountOfProfilesNeeded = 0;

        for (const auto& entry: profilesByRole)
        {
            const auto profile = entry.second;
            if (profile == boost::none)
                ++amountOfProfilesNeeded;
        }

        if (amountOfProfilesNeeded + totalProfileNumber > m_maxProfileCount)
        {
            return CameraDiagnostics::CameraInvalidParams(
                lit("- can not create profiles. Please delete %1 profile%2 on the camera web page")
                    .arg(amountOfProfilesNeeded)
                    .arg(amountOfProfilesNeeded > 1 ? lit("s") : QString()));
        }
    }

    for (auto& entry: profilesByRole)
    {
        const auto role = entry.first;
        auto& profile = entry.second;

        if (profile == boost::none)
        {
            profile = HanwhaVideoProfile();
            result = createNxProfile(
                role,
                &profile->number,
                totalProfileNumber,
                &profilesToRemove);

            if (!result)
                return result;
        }
        else
        {
            updateProfileNameIfNeeded(role, *profile);
        }
    }

    for (const auto& entry: profilesByRole)
    {
        const auto role = entry.first;
        const auto& profile = entry.second;

        NX_VERBOSE(
            this,
            lm("Direct profile %1 has been selected for role %2 for %3 (%4)")
                .args(profile->number, role, getName(), getId()));

        setDirectProfile(role, profile->number);
        // We set 'Record' profile policy to the primary profile and use
        // this profile for an archive connection.
        if (role == Qn::ConnectionRole::CR_LiveVideo)
            setDirectProfile(Qn::ConnectionRole::CR_Archive, profile->number);
    }

    if (isBypassSupported())
    {
        result = findProfiles(
            &profilesByRole[Qn::ConnectionRole::CR_LiveVideo],
            &profilesByRole[Qn::ConnectionRole::CR_SecondaryLiveVideo],
            &totalProfileNumber,
            &profilesToRemove,
            /*useBypass*/ true);

        if (!result)
            return result;

        for (const auto& entry: profilesByRole)
        {
            const auto role = entry.first;
            const auto& profile = entry.second;

            if (profile == boost::none)
            {
                return CameraDiagnostics::CameraInvalidParams(
                    lit("Can't fetch profile number via bypass."));
            }

            NX_VERBOSE(
                this,
                lm("Bypass profile %1 has been selected for role %2 for %3 (%4)")
                    .args(profile->number, role, getName(), getId()));

            setBypassProfile(role, profile->number);
            if (role == Qn::ConnectionRole::CR_LiveVideo)
                setBypassProfile(Qn::ConnectionRole::CR_Archive, profile->number);
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::fetchExistingProfiles()
{
    m_profileByRole.clear();

    HanwhaRequestHelper helper(sharedContext());
    const auto response = helper.view(
        lit("media/videoprofilepolicy"),
        {{kHanwhaChannelProperty, QString::number(getChannel())}});

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            response.requestUrl(),
            lit("Can't read video profile policy"));
    }

    std::set<int> availableProfiles;
    for (const auto& profile: response.response())
    {
        bool isOk = false;
        const auto profileNumber = profile.second.toInt(&isOk);
        if (!isOk)
            continue;

        if (profile.first.endsWith(lit("Profile")))
            availableProfiles.insert(profile.second.toInt());

        // Accodring to hawna request for ONVIF:
        // - Use Recording profile from NVR as a Primary stream.
        // - Use Live Streaming profile from NVR as a Secondary stream.
        // See: http://git.wisenetdev.com/HanwhaTechwinAmerica/WAVE/issues/290
        if (profile.first.endsWith(lit("RecordProfile")))
        {
            setDirectProfile(Qn::ConnectionRole::CR_LiveVideo, profileNumber);
            setDirectProfile(Qn::ConnectionRole::CR_Archive, profileNumber);
        }
        else if (profile.first.endsWith(lit("LiveProfile")))
        {
            setDirectProfile(Qn::ConnectionRole::CR_SecondaryLiveVideo, profileNumber);
        }
    }

    // Select the best avaliable profile, for primary live and archive.
    if (m_profileByRole.find(Qn::ConnectionRole::CR_LiveVideo) == m_profileByRole.end())
    {
        const auto response = helper.view(
            lit("media/videoprofile"),
            {{kHanwhaChannelProperty, QString::number(getChannel())}});

        int bestProfile = kHanwhaInvalidProfile;
        int bestScore = 0;
        const auto& channelProfiles = parseProfiles(response).at(getChannel());
        for (const auto& profileEntry: channelProfiles)
        {
            const auto profile = profileEntry.second;
            const auto codecCoefficient =
                kHanwhaCodecCoefficients.find(profile.codec) != kHanwhaCodecCoefficients.cend()
                ? kHanwhaCodecCoefficients.at(profile.codec)
                : -1;

            if (!availableProfiles.count(profile.number))
                continue;

            const auto score = profile.resolution.width()
                * profile.resolution.height()
                * codecCoefficient
                + profile.frameRate;

            if (score > bestScore)
            {
                bestScore = score;
                bestProfile = profile.number;
            }
        }

        if (bestProfile != kHanwhaInvalidProfile)
        {
            setDirectProfile(Qn::ConnectionRole::CR_LiveVideo, bestProfile);
            setDirectProfile(Qn::ConnectionRole::CR_Archive, bestProfile);
        }
    }

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

CameraDiagnostics::Result HanwhaResource::setUpProfilePolicies(
    int primaryProfile,
    int secondaryProfile)
{
    HanwhaRequestHelper helper(sharedContext());
    const auto response = helper.set(
        lit("media/videoprofilepolicy"),
        {
            {kHanwhaChannelProperty, QString::number(getChannel())},
            {lit("LiveProfile"), QString::number(secondaryProfile)},
            {lit("RecordProfile"), QString::number(primaryProfile)},
            {lit("NetworkProfile"), QString::number(secondaryProfile)}
        });

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            response.requestUrl(),
            lit("can't set up profile dependencies"));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::findProfiles(
    boost::optional<HanwhaVideoProfile>* outPrimaryProfile,
    boost::optional<HanwhaVideoProfile>* outSecondaryProfile,
    int* totalProfileNumber,
    std::set<int>* profilesToRemoveIfProfilesExhausted,
    bool useBypass)
{
    if (profilesToRemoveIfProfilesExhausted)
        profilesToRemoveIfProfilesExhausted->clear();

    if (outPrimaryProfile)
        *outPrimaryProfile = boost::none;

    if (outSecondaryProfile)
        *outSecondaryProfile = boost::none;

    HanwhaProfileMap profiles;
    const auto result = fetchProfiles(&profiles, useBypass);

    if (!result)
        return result;

    if (profiles.empty())
        return CameraDiagnostics::NoErrorResult();

    if (totalProfileNumber)
        *totalProfileNumber = (int) profiles.size();

    static const auto kAppName = QnAppInfo::productNameLong();
    if (outPrimaryProfile)
        *outPrimaryProfile = findProfile(profiles, Qn::ConnectionRole::CR_LiveVideo, kAppName);

    if (outSecondaryProfile)
    {
        *outSecondaryProfile = findProfile(
            profiles,
            Qn::ConnectionRole::CR_SecondaryLiveVideo,
            kAppName);
    }

    if (profilesToRemoveIfProfilesExhausted)
    {
        *profilesToRemoveIfProfilesExhausted = findProfilesToRemove(
            profiles,
            outPrimaryProfile ? *outPrimaryProfile : boost::none,
            outSecondaryProfile ? *outSecondaryProfile : boost::none);
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::fetchProfiles(
    HanwhaProfileMap* outProfiles,
    bool useBypass)
{
    NX_ASSERT(outProfiles);
    if (!outProfiles)
        return CameraDiagnostics::PluginErrorResult("Output profiles isn't profvided");

    HanwhaRequestHelper helper(sharedContext(), useBypass ? bypassChannel() : boost::none);
    const auto response = helper.view(lit("media/videoprofile"));

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                response.requestUrl(),
                response.errorString()));
    }

    const auto profileByChannel = parseProfiles(
        response,
        useBypass ? bypassChannel() : boost::none);

    const auto currentChannelProfiles = profileByChannel.find(getChannel());
    *outProfiles = currentChannelProfiles == profileByChannel.cend()
        ? HanwhaProfileMap()
        : currentChannelProfiles->second;

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::removeProfile(int profileNumber)
{
    HanwhaRequestHelper helper(sharedContext());
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
            CameraDiagnostics::RequestFailedResult(response.requestUrl(), response.errorString()));
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
        return CameraDiagnostics::PluginErrorResult(
            lit("Create profile: output profile number is null"));
    }

    QnLiveStreamParams streamParameters;
    const auto fps = role == Qn::ConnectionRole::CR_LiveVideo
        ? kMaxPossibleFps
        : QnSecurityCamResource::kDefaultSecondStreamFpsMedium;

    streamParameters.codec = streamCodec(role);
    streamParameters.fps = streamFrameRate(role, fps);
    streamParameters.resolution = streamResolution(role);
    streamParameters.quality = Qn::StreamQuality::high;
    streamParameters.bitrateKbps = streamBitrate(role, streamParameters);

    HanwhaProfileParameterFlags profileParameterFlags = HanwhaProfileParameterFlag::newProfile;
    if (isAudioSupported())
        profileParameterFlags |= HanwhaProfileParameterFlag::audioSupported;

    auto profileParameters = makeProfileParameters(
        role,
        streamParameters,
        profileParameterFlags);

    HanwhaRequestHelper helper(sharedContext(), bypassChannel());
    auto response = helper.add(lit("media/videoprofile"), profileParameters);

    // NVRs can report invalid maximum profile length.
    const bool needToTryAgain = !response.isSuccessful()
        && isNvr()
        && !isBypassSupported()
        && response.errorCode() == kHanwhaInvalidInputValue;

    if (needToTryAgain)
    {
        profileParameters[kHanwhaProfileNameProperty] =
            nxProfileName(role, kHanwhaProfileNameDefaultMaxLength);

        response = helper.add(lit("media/videoprofile"), profileParameters);
    }

    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(response.requestUrl(), response.errorString()));
    }

    // Creation of profiles on the NVR channel has some peculiarities:
    // 1. If profiles are being created via NVR CGI profile number isn't returned in the response.
    // 2. NVRs can return garbage instead of actual profiles if make 'view'
    //  request too fast after profile 'add' or 'update' request. (actual for both types of NVR -
    //  with bypass and without)
    // 3. Profile can be created with incorrect settings (fps, resolution)
    //  so we need to update it after creation (actual for NVRs without bypass).
    // We need to wait a little after profile adding/updating and then try to verify
    // newly created profile.
    if (isNvr())
    {
        static const int kMaxUpdateProfileTries = 10;
        *outProfileNumber = kHanwhaInvalidProfile;
        for (auto i = 0; i < kMaxUpdateProfileTries; ++i)
        {
            if (commonModule()->isNeedToStop())
                return CameraDiagnostics::ServerTerminatedResult();

            const auto profileNumber = verifyProfile(role);
            if (profileNumber != boost::none)
            {
                *outProfileNumber = *profileNumber;
                break;
            }
        }

        if (*outProfileNumber == kHanwhaInvalidProfile)
        {
            return CameraDiagnostics::CameraInvalidParams(
                lm("Can't verify profile for %1 stream.")
                    .args(role == Qn::ConnectionRole::CR_LiveVideo
                        ? lit("primary")
                        : lit("secondary")));
        }
    }

    if (isNvr() && !isBypassSupported())
    {
        profileParameters.erase(kHanwhaProfileNameProperty);
        profileParameters.emplace(kHanwhaProfileNumberProperty, QString::number(*outProfileNumber));

        response = helper.update(lit("media/videoprofile"), profileParameters);
        if (!response.isSuccessful())
        {
            return CameraDiagnostics::RequestFailedResult(
                response.requestUrl(),
                lit("Can't update profile"));
        }
    }
    else
    {
        bool success = false;
        *outProfileNumber = response.response()[kHanwhaProfileNumberProperty].toInt(&success);

        if (!success)
            return CameraDiagnostics::CameraInvalidParams(lit("Invalid profile number string"));
    }

    return CameraDiagnostics::NoErrorResult();
}

boost::optional<int> HanwhaResource::verifyProfile(Qn::ConnectionRole role)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    boost::optional<HanwhaVideoProfile> profile;
    const auto result = findProfiles(
        role == Qn::ConnectionRole::CR_LiveVideo ? &profile : nullptr,
        role == Qn::ConnectionRole::CR_SecondaryLiveVideo ? &profile : nullptr,
        /*totalProfileNumber*/ nullptr,
        /*profilesToRemove*/ nullptr);

    if (!result || profile == boost::none)
    {
        NX_VERBOSE(
            this,
            lm("Profile verification is failed because profile for %1 stream "
                "can't be found. Device: %2 (%3)")
                .args(
                    role == Qn::ConnectionRole::CR_LiveVideo
                        ? lit("primary")
                        : lit("secondary"),
                    getName(),
                    getId()));
        return boost::none;
    }

    const bool isIncorrectProfile = profile->number == kHanwhaInvalidProfile
        || profile->frameRate <= 0
        || profile->bitrateKbps <= 0
        || profile->codec == AVCodecID::AV_CODEC_ID_NONE;

    if (isIncorrectProfile)
    {
        NX_VERBOSE(
            this,
            lm("Profile verification is failed because profile for %1 stream "
                "has incorrect parameters. Device: %2 (%3)")
                .args(
                    role == Qn::ConnectionRole::CR_LiveVideo
                        ? lit("primary")
                        : lit("secondary"),
                    getName(),
                    getId()));
        return boost::none;
    }

    return profile->number;
}

CameraDiagnostics::Result HanwhaResource::updateProfileNameIfNeeded(
    Qn::ConnectionRole role,
    const HanwhaVideoProfile& profile)
{
    const auto properProfileName = nxProfileName(role);
    bool needToUpdateProfileName =
        (profile.name == nxProfileName(role, kHanwhaProfileNameDefaultMaxLength))
        && (nxProfileName(role, kHanwhaProfileNameDefaultMaxLength) != properProfileName);

    if (needToUpdateProfileName)
    {
        // Try to update profile name and silently ignore any errors.
        HanwhaRequestHelper helper(sharedContext(), bypassChannel());
        const auto response = helper.update(
            lit("media/videoprofile"),
            {
                {kHanwhaChannelProperty, QString::number(getChannel())},
                {kHanwhaProfileNumberProperty, QString::number(profile.number)},
                {kHanwhaProfileNameProperty, properProfileName}
            });

        if (!response.isSuccessful())
        {
            NX_WARNING(
                this,
                lm("Can't update %1 profile name for %2 (%3)")
                    .args(
                        (role == Qn::ConnectionRole::CR_LiveVideo
                            ? lit("primary")
                            : lit("secondary")),
                        getName(),
                        getId()));
        }
    }

    return CameraDiagnostics::NoErrorResult();
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

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaResource::fetchCodecInfo(HanwhaCodecInfo* outCodecInfo)
{
    if (isBypassSupported())
    {
        HanwhaRequestHelper helper(sharedContext(), bypassChannel());
        helper.setGroupBy(kHanwhaChannelProperty);
        const auto response = helper.view(lit("media/videocodecinfo"));

        if (!response.isSuccessful())
        {
            return CameraDiagnostics::RequestFailedResult(
                response.requestUrl(),
                lit("Can't fetch codec info via bypass"));
        }

        *outCodecInfo = HanwhaCodecInfo(response, cgiParameters());
        if (!outCodecInfo->isValid())
            return CameraDiagnostics::CameraInvalidParams(lit("Can't fetch bypass codec info"));

        outCodecInfo->updateToChannel(getChannel());
    }
    else
    {
        auto codecInfo = sharedContext()->videoCodecInfo();
        if (!codecInfo)
            return codecInfo.diagnostics;

        *outCodecInfo = codecInfo.value;
    }
    return CameraDiagnostics::NoErrorResult();
}

void HanwhaResource::cleanUpOnProxiedDeviceChange()
{
    setProperty(kPrimaryStreamResolutionParamName, QString());
    setProperty(kSecondaryStreamResolutionParamName, QString());
    setProperty(kPrimaryStreamCodecParamName, QString());
    setProperty(kPrimaryStreamCodecProfileParamName, QString());
    setProperty(kSecondaryStreamCodecParamName, QString());
    setProperty(kSecondaryStreamCodecProfileParamName, QString());
    setProperty(kPrimaryStreamGovLengthParamName, QString());
    setProperty(kSecondaryStreamGovLengthParamName, QString());
    setProperty(kPrimaryStreamBitrateControlParamName, QString());
    setProperty(kSecondaryStreamBitrateControlParamName, QString());
    setProperty(kPrimaryStreamBitrateParamName, QString());
    setProperty(kSecondaryStreamBitrateParamName, QString());
    setProperty(kPrimaryStreamEntropyCodingParamName, QString());
    setProperty(kSecondaryStreamEntropyCodingParamName, QString());
    setProperty(kPrimaryStreamFpsParamName, QString());
    setProperty(kSecondaryStreamFpsParamName, QString());
}

AVCodecID HanwhaResource::streamCodec(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? kPrimaryStreamCodecParamName
        : kSecondaryStreamCodecParamName;

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
        ? kPrimaryStreamCodecProfileParamName
        : kSecondaryStreamCodecProfileParamName;

    const QString profile = getProperty(propertyName);
    return suggestCodecProfile(codec, role, profile);
}

QSize HanwhaResource::streamResolution(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? kPrimaryStreamResolutionParamName
        : kSecondaryStreamResolutionParamName;

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
        userDefinedFps = getProperty(kSecondaryStreamFpsParamName).toInt();
    else if (role == Qn::ConnectionRole::CR_LiveVideo && isNvr() && isConnectedViaSunapi())
        userDefinedFps = getProperty(kPrimaryStreamFpsParamName).toInt();

    return closestFrameRate(role, userDefinedFps ? userDefinedFps : desiredFps);
}

int HanwhaResource::streamGovLength(Qn::ConnectionRole role) const
{
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? kPrimaryStreamGovLengthParamName
        : kSecondaryStreamGovLengthParamName;

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
        ? kPrimaryStreamBitrateControlParamName
        : kSecondaryStreamBitrateControlParamName;

    QString bitrateControlString = getProperty(propertyName);
    if (bitrateControlString.isEmpty())
        return defaultBitrateControlForStream(role);

    auto result = fromHanwhaString<Qn::BitrateControl>(bitrateControlString);
    if (result == Qn::BitrateControl::undefined)
        return defaultBitrateControlForStream(role);

    return result;
}

int HanwhaResource::streamBitrate(
    Qn::ConnectionRole role,
    const QnLiveStreamParams& liveStreamParams) const
{
    QnLiveStreamParams streamParams = liveStreamParams;
    const auto propertyName = role == Qn::ConnectionRole::CR_LiveVideo
        ? kPrimaryStreamBitrateParamName
        : kSecondaryStreamBitrateParamName;

    const QString bitrateString = getProperty(propertyName);
    int bitrateKbps = bitrateString.toInt();
    streamParams.resolution = streamResolution(role);
    if (bitrateKbps == 0)
    {
        // Since we can't fully control bitrate on the NVRs that don't have bypass
        // we use default bitrate with 1.0 (QualityNormal) coefficient.
        if (isNvr() && !isBypassSupported())
            streamParams.quality = Qn::StreamQuality::normal;

        bitrateKbps = nx::vms::server::resource::Camera::suggestBitrateKbps(streamParams, role);
    }

    auto streamCapability = cameraMediaCapability()
        .streamCapabilities
        .value(role == Qn::ConnectionRole::CR_LiveVideo
            ? Qn::StreamIndex::primary
            : Qn::StreamIndex::secondary);

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

int HanwhaResource::profileByRole(Qn::ConnectionRole role, bool isBypassProfile) const
{
    if (isNvr() && role == Qn::CR_Archive)
        return kHanwhaInvalidProfile;

    auto itr = m_profileByRole.find(role);
    if (itr != m_profileByRole.cend())
    {
        const auto& profileNumbers = itr->second;
        return isBypassProfile
            ? profileNumbers.bypassNumber
            : profileNumbers.directNumber;
    }

    return kHanwhaInvalidProfile;
}

AVCodecID HanwhaResource::defaultCodecForStream(Qn::ConnectionRole /*role*/) const
{
    const auto& codecs = m_codecInfo.codecs(getChannel());

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
    const auto& resolutions = m_codecInfo.resolutions(
        getChannel(),
        defaultCodecForStream(role),
        lit("General"));

    if (resolutions.empty())
        return QSize();

    if (role == Qn::ConnectionRole::CR_LiveVideo)
        return resolutions[0];

    return bestSecondaryResolution(resolutions[0], resolutions);
}

int HanwhaResource::defaultGovLengthForStream(Qn::ConnectionRole role) const
{
    return mediaCapabilityForRole(role).maxFps;
}

Qn::BitrateControl HanwhaResource::defaultBitrateControlForStream(
    Qn::ConnectionRole /*role*/) const
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

    const auto entropyCodingParameter = cgiParameters().parameter(
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

    const auto codecProfileParameter = cgiParameters().parameter(
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
        closestFrameRate(role, defaultSecondaryFps(Qn::StreamQuality::normal));

    return kHanwhaInvalidFps;
}

QString HanwhaResource::defaultValue(const QString& parameter, Qn::ConnectionRole role) const
{
    if (parameter.endsWith(kEncodingTypeProperty))
        return toHanwhaString(defaultCodecForStream(role));
    else if (parameter.endsWith(kResolutionProperty))
        return toHanwhaString(defaultResolutionForStream(role));
    else if (parameter.endsWith(kBitrateControlTypeProperty))
        return toHanwhaString(defaultBitrateControlForStream(role));
    else if (parameter.endsWith(kGovLengthProperty))
        return QString::number(defaultGovLengthForStream(role));
    else if (parameter.endsWith(kCodecProfileProperty))
        return defaultCodecProfileForStream(role);
    else if (parameter.endsWith(kEntropyCodingProperty))
        return toHanwhaString(defaultEntropyCodingForStream(role));
    else if (parameter.endsWith(kBitrateProperty))
    {
        auto camera = serverModule()->videoCameraPool()->getVideoCamera(toSharedPointer());
        if (!camera)
            return QString::number(defaultBitrateForStream(role));

        QnLiveStreamProviderPtr provider = role == Qn::ConnectionRole::CR_LiveVideo
            ? camera->getPrimaryReader()
            : camera->getSecondaryReader();

        if (!provider)
            QString::number(defaultBitrateForStream(role));

        const auto liveStreamParameters = provider->getLiveParams();
        return QString::number(streamBitrate(role, liveStreamParameters));
    }
    else if (parameter.endsWith(kFramePriorityProperty))
        return lit("FrameRate");

    return QString();
}

QString HanwhaResource::suggestCodecProfile(
    AVCodecID codec,
    Qn::ConnectionRole role,
    const QString& desiredProfile) const
{
    const auto& profiles = m_codecInfo.codecProfiles(codec);
    if (profiles.contains(desiredProfile))
        return desiredProfile;

    return defaultCodecProfileForStream(role);
}

QSize HanwhaResource::bestSecondaryResolution(
    const QSize& primaryResolution,
    const std::vector<QSize>& resolutionList) const
{
    if (primaryResolution.isEmpty())
    {
        NX_WARNING(
            this,
            lit("Primary resolution height is 0. Can not determine secondary resolution"));

        return QSize();
    }

    QList<QSize> resolutions; //< TODO: #dmishin get rid of this.
    for (const auto& resolution: resolutionList)
        resolutions.push_back(resolution);

    return Camera::closestSecondaryResolution(
        getResolutionAspectRatio(primaryResolution),
        resolutions);
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

        if (info->isService()) //< E.g, "Reset profiles to default" button.
        {
            supportedIds.insert(id);
            continue;
        }

        const auto neededPtzCapabilities = info->ptzCapabilities();
        if (neededPtzCapabilities != Ptz::NoPtzCapabilities)
        {
            const bool hasNeededCapabilities =
                (neededPtzCapabilities & ptzCapabilities(core::ptz::Type::configurational))
                    == neededPtzCapabilities;

            if (hasNeededCapabilities)
                supportedIds.insert(id);

            continue;
        }

        const bool needToCheck = parameter.dataType == QnCameraAdvancedParameter::DataType::Number
            || parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration;

        if (needToCheck && parameter.range.isEmpty())
            continue;

        if (info->hasParameter())
        {
            const auto& cgiParams = cgiParameters();
            boost::optional<HanwhaCgiParameter> cgiParameter;
            const auto rangeParameter = info->rangeParameter();

            if (rangeParameter.isEmpty())
            {
                cgiParameter = cgiParams.parameter(
                    info->cgi(),
                    info->submenu(),
                    info->updateAction(),
                    info->parameterName());
            }
            else
            {
                cgiParameter = cgiParams.parameter(rangeParameter);
            }

            if (!cgiParameter)
                continue;

            const auto parameterValue = info->parameterValue();
            if (!parameterValue.isEmpty() && !cgiParameter->isValueSupported(parameterValue))
                continue;
        }

        bool isSupported = true;
        auto supportAttribute = info->supportAttribute();

        const auto& attrs = attributes();
        if (!supportAttribute.isEmpty())
        {
            isSupported = false;
            if (!info->isChannelIndependent())
                supportAttribute += lit("/%1").arg(isBypassSupported() ? 0 : getChannel());

            const auto boolAttribute = attrs.attribute<bool>(supportAttribute);
            if (boolAttribute != boost::none)
            {
                isSupported = boolAttribute.get();
            }
            else
            {
                const auto intAttribute = attrs.attribute<int>(supportAttribute);
                if (intAttribute != boost::none)
                    isSupported = intAttribute.get() > 0;
            }
        }

        if (isSupported)
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
    NX_ASSERT(inOutParameter);
    if (!inOutParameter)
        return false;

    const auto info = advancedParameterInfo(inOutParameter->id);
    if (!info)
        return false;

    const auto parameterName = info->parameterName();
    if (parameterName == kHanwhaBitrateProperty)
        return addBitrateRanges(inOutParameter, *info);

    if (parameterName == kHanwhaFrameRateProperty)
        return addFrameRateRanges(inOutParameter, *info);

    if (parameterName == kHanwhaResolutionProperty)
        return addResolutionRanges(inOutParameter, *info);

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
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::range;
            dependency.range = lit("%1,%2").arg(minLimit).arg(maxLimit);
            dependency.autoFillId();
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
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::range;
            dependency.range = lit("1,%2").arg(limits.maxFps);
            dependency.autoFillId();
            return dependency;
        };

    return addDependencies(inOutParameter, info, createDependencyFunc);
}

bool HanwhaResource::addResolutionRanges(
    QnCameraAdvancedParameter* inOutParameter,
    const HanwhaAdavancedParameterInfo& info) const
{
    const auto codecs = m_codecInfo.codecs(getChannel());
    const auto streamPrefix =
        info.profileDependency() == Qn::ConnectionRole::CR_LiveVideo
            ? "PRIMARY%"
            : "SECONDARY%";

    for (const auto& codec: codecs)
    {
        const auto codecString = toHanwhaString(codec);
        QnCameraAdvancedParameterCondition codecCondition;
        codecCondition.type = QnCameraAdvancedParameterCondition::ConditionType::equal;
        codecCondition.paramId = lit("%1media/videoprofile/EncodingType")
            .arg(streamPrefix);
        codecCondition.value = codecString;

        const auto resolutions = m_codecInfo.resolutions(getChannel(), codec, "General");
        QString resolutionRangeString;
        for (const auto& resolution: resolutions)
        {
            resolutionRangeString +=
                QString("%1x%2").arg(resolution.width()).arg(resolution.height()) + ",";
        }

        if (!resolutionRangeString.isEmpty())
            resolutionRangeString.chop(1);

        QnCameraAdvancedParameterDependency dependency;
        dependency.type = QnCameraAdvancedParameterDependency::DependencyType::range;
        dependency.range = resolutionRangeString;
        dependency.conditions.push_back(codecCondition);
        dependency.autoFillId();

        inOutParameter->dependencies.push_back(dependency);
    }

    return true;
}

bool HanwhaResource::addDependencies(
    QnCameraAdvancedParameter* inOutParameter,
    const HanwhaAdavancedParameterInfo& info,
    CreateDependencyFunc createDependencyFunc) const
{
    NX_ASSERT(inOutParameter);
    if (!inOutParameter)
        return false;

    const auto channel = getChannel();
    const auto codecs = m_codecInfo.codecs(channel);

    const auto streamPrefix = info.profileDependency() == Qn::ConnectionRole::CR_LiveVideo
        ? lit("PRIMARY%")
        : lit("SECONDARY%");

    for (const auto& codec: codecs)
    {
        const auto resolutions = m_codecInfo.resolutions(channel, codec, lit("General"));
        for (const auto& resolution : resolutions)
        {
            auto limits = m_codecInfo.limits(channel, codec, lit("General"), resolution);

            const auto codecString = toHanwhaString(codec);
            QnCameraAdvancedParameterCondition codecCondition;
            codecCondition.type = QnCameraAdvancedParameterCondition::ConditionType::equal;
            codecCondition.paramId = lit("%1media/videoprofile/EncodingType")
                .arg(streamPrefix);
            codecCondition.value = codecString;

            const auto resolutionString = toHanwhaString(resolution);
            QnCameraAdvancedParameterCondition resolutionCondition;
            resolutionCondition.type =
                QnCameraAdvancedParameterCondition::ConditionType::equal;
            resolutionCondition.paramId = lit("%1media/videoprofile/Resolution")
                .arg(streamPrefix);
            resolutionCondition.value = resolutionString;

            QStringList bitrateControlTypeList;
            const auto bitrateControlTypes = cgiParameters().parameter(
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
                    = QnCameraAdvancedParameterCondition::ConditionType::equal;
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

    setDefaultGroupName(getModel());
    setGroupId(getPhysicalId().split('_')[0]);
    setPhysicalId(physicalIdForChannel(getGroupId(), value));

    const auto suffix = lit("-channel %1").arg(value + 1);
    if (value > 0 && !getName().endsWith(suffix))
        setName(getName() + suffix);
}

QString HanwhaResource::toHanwhaAdvancedParameterValue(
    const QnCameraAdvancedParameter& parameter,
    const HanwhaAdavancedParameterInfo& /*parameterInfo*/,
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
    const HanwhaAdavancedParameterInfo& /*parameterInfo*/,
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
    auto camera = serverModule()->videoCameraPool()->getVideoCamera(toSharedPointer(this));
    if (!camera)
        return;

    static const auto reopen =
        [](const QnLiveStreamProviderPtr& stream)
        {
            if (stream && stream->isRunning())
                stream->pleaseReopenStream();
        };

    if (reopenPrimary)
        reopen(camera->getPrimaryReader());

    if (reopenSecondary)
        reopen(camera->getSecondaryReader());
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
    QnCameraAdvancedParamValueList groupLeadValues = getApiParameters(groupLeadsToFetch).toValueList();
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

QnCameraAdvancedParamValueList HanwhaResource::addAssociatedParameters(
    const QnCameraAdvancedParamValueList& values)
{
    std::map<QString, QString> parameterValues;
    for (const auto& value: values)
        parameterValues[value.id] = value.value;

    QSet<QString> parametersToFetch;
    for (const auto& entry: parameterValues)
    {
        const auto& id = entry.first;

        const auto info = advancedParameterInfo(id);
        if (!info)
            continue;

        const auto associatedParameters = info->associatedParameters();
        if (associatedParameters.empty())
            continue;

        for (const auto& associatedParameter: associatedParameters)
        {
            if (parameterValues.find(associatedParameter) != parameterValues.cend())
                continue; //< Parameter is already present.

            parametersToFetch.insert(associatedParameter);
        }
    }

    auto associatedParameterValues = getApiParameters(parametersToFetch);
    associatedParameterValues.appendValueList(values);

    return associatedParameterValues.toValueList();
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
        const auto parameter = m_advancedParametersProvider.getParameterById(parameterValue.id);
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
    const auto parameter = m_advancedParametersProvider.getParameterById(command.id);
    if (!parameter.isValid())
        return false;

    const auto info = advancedParameterInfo(command.id);
    if (!info)
        return false;

    if (info->isService())
        return executeServiceCommand(parameter, *info);

    HanwhaRequestHelper::Parameters requestParameters;
    if (!info->parameterName().isEmpty())
    {
        const auto cgiParameter = cgiParameters().parameter(
            info->cgi(),
            info->submenu(),
            info->updateAction(),
            info->parameterName());

        if (!cgiParameter)
            return false;

        const auto requestedParameterValues = info->parameterValue()
            .split(L',', QString::SplitBehavior::SkipEmptyParts);

        if (cgiParameter->type() == HanwhaCgiParameterType::enumeration)
        {
            QStringList parameterValues;
            const auto possibleValues = cgiParameter->possibleValues();
            for (const auto& requestedValue: requestedParameterValues)
            {
                if (possibleValues.contains(requestedValue))
                    parameterValues.push_back(requestedValue);
            }

            if (!parameterValues.isEmpty())
                requestParameters.emplace(info->parameterName(), parameterValues.join(L','));
        }
        else if (cgiParameter->type() == HanwhaCgiParameterType::boolean)
        {
            NX_ASSERT(
                requestedParameterValues.size() == 1,
                "Boolean parameters support only single 'True/False' value");
            if (requestedParameterValues.size() == 1)
                requestParameters.emplace(info->parameterName(), requestedParameterValues.at(0));
        }
    }

    return executeCommandInternal(*info, requestParameters);
}

bool HanwhaResource::executeCommandInternal(
    const HanwhaAdavancedParameterInfo& info,
    const HanwhaRequestHelper::Parameters& parameters)
{
    auto makeRequest =
        [&info, this](HanwhaRequestHelper::Parameters parameters, int channel)
        {
            if (channel != kHanwhaInvalidChannel)
                parameters[kHanwhaChannelProperty] = QString::number(channel);

            HanwhaRequestHelper helper(sharedContext(), bypassChannel());
            const auto response = helper.doRequest(
                info.cgi(),
                info.submenu(),
                info.updateAction(),
                nx::utils::RwLockType::write,
                parameters);

            return response.isSuccessful();
        };

    if (info.shouldAffectAllChannels())
    {
        // TODO: #dmishin this will not work with proxied multichannel cameras.
        const auto& systemInfo = sharedContext()->information();
        if (!systemInfo)
            return false;

        bool result = true;
        const auto channelCount = systemInfo->channelCount;
        for (auto i = 0; i < channelCount; ++i)
        {
            result = makeRequest(parameters, i);
            if (!result)
                return false;
        }

        return result;
    }
    else if (!info.isChannelIndependent())
    {
        return makeRequest(parameters, getChannel());
    }

    return makeRequest(parameters, kHanwhaInvalidChannel);
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
        setProperty(kSecondaryStreamFpsParamName, defaultFrameRateForStream(role));
        setProperty(kSecondaryStreamBitrateParamName, defaultBitrateForStream(role));
    }

    saveProperties();
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

boost::optional<HanwhaResource::HanwhaPortInfo> HanwhaResource::portInfoFromId(
    const QString& id) const
{
    HanwhaPortInfo result;
    auto split = id.split(L'.');
    if (split.size() != 2 && split.size() != 3)
        return boost::none;

    result.isProxied = split.size() == 3 && split[0] == kBypassPrefix;
    result.prefix = split[result.isProxied ? 1 : 0];
    result.number = split[result.isProxied ? 2 : 1];
    result.submenu = result.prefix.toLower();

    return result;
}

bool HanwhaResource::setOutputPortStateInternal(const QString& outputId, bool activate)
{
    const auto info = portInfoFromId(outputId.isEmpty() ? m_defaultOutputPortId : outputId);
    if (info == boost::none)
        return false;

    const auto state = activate ? lit("On") : lit("Off");

    HanwhaRequestHelper::Parameters parameters =
        {{lit("%1.%2.State").arg(info->prefix).arg(info->number), state}};

    if (info->submenu == lit("alarmoutput"))
    {
        parameters.emplace(
            lit("%1.%2.ManualDuration")
                .arg(info->prefix)
                .arg(info->number),
            lit("Always"));
    }

    HanwhaRequestHelper helper(sharedContext(), info->isProxied ? bypassChannel() : boost::none);
    const auto response = helper.control(
        lit("io/%1").arg(info->submenu),
        parameters);

    return response.isSuccessful();
}

const HanwhaAttributes& HanwhaResource::attributes() const
{
    if (!isBypassSupported())
        return m_attributes;

    return m_bypassDeviceAttributes;
}

const HanwhaCgiParameters& HanwhaResource::cgiParameters() const
{
    if (!isBypassSupported())
        return m_cgiParameters;

    return m_bypassDeviceCgiParameters;
}

boost::optional<int> HanwhaResource::bypassChannel() const
{
    if (isBypassSupported())
        return getChannel();

    return boost::none;
}

CameraDiagnostics::Result HanwhaResource::enableAudioInput()
{
    const auto audioInputEnabledParameter =
        m_cgiParameters.parameter("media/audioinput/set/Enable");

    if (!audioInputEnabledParameter)
    {
        NX_DEBUG(
            this,
            "media/audioinput/set/Enable parameter is not supported by device: %1 (%2)",
            getUserDefinedName(), getId());

        return CameraDiagnostics::NoErrorResult();
    }

    HanwhaRequestHelper helper(sharedContext());
    const auto response = helper.set("media/audioinput", {
        {"Enable", kHanwhaTrue},
        {"Channel", QString::number(getChannel())}
    });

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            response.requestUrl(), response.errorString());
    }

    return CameraDiagnostics::NoErrorResult();
}

bool HanwhaResource::isNvr() const
{
    return m_deviceType == HanwhaDeviceType::nvr;
}

bool HanwhaResource::isProxiedAnalogEncoder() const
{
    return bypassDeviceType() == HanwhaDeviceType::encoder;
}

HanwhaDeviceType HanwhaResource::deviceType() const
{
    return m_deviceType;
}

HanwhaDeviceType HanwhaResource::bypassDeviceType() const
{
    return m_bypassDeviceType;
}

bool HanwhaResource::hasSerialPort() const
{
    return m_hasSerialPort;
}

QString HanwhaResource::nxProfileName(
    Qn::ConnectionRole role,
    boost::optional<int> forcedProfileNameLength) const
{
    auto maxLength = forcedProfileNameLength == boost::none
        ? kHanwhaProfileNameDefaultMaxLength
        : forcedProfileNameLength.get();

    if (forcedProfileNameLength == boost::none)
    {
        static const std::vector<QString> parametersToCheck = {
            lit("media/videoprofile/add_update/Name"),
            lit("media/videoprofile/add/Name")
        };

        for (const auto& parameterToCheck: parametersToCheck)
        {
            const auto nxProfileNameParameter = cgiParameters().parameter(parameterToCheck);
            if (nxProfileNameParameter != boost::none && nxProfileNameParameter->maxLength() > 0)
            {
                maxLength = nxProfileNameParameter->maxLength();
                break;
            }
        }
    }

    const auto suffix = profileSuffixByRole(role);
    const auto appName = profileFullProductName(QnAppInfo::productNameLong())
        .left(maxLength - suffix.length());

    return appName + suffix;
}

bool HanwhaResource::needToReplaceProfile(
    const boost::optional<HanwhaVideoProfile>& nxProfileToReplace,
    Qn::ConnectionRole role) const
{
    // If we have no profile yet.
    if (nxProfileToReplace == boost::none)
        return true;

    // We want to use new profile instead of obsolete (e.g. WAVESecondary instead of WAVSecondary).
    if (nxProfileToReplace->name == nxProfileName(role, kHanwhaProfileNameDefaultMaxLength))
        return true;

    return false;
}

std::shared_ptr<HanwhaSharedResourceContext> HanwhaResource::sharedContext() const
{
    QnMutexLocker lock(&m_mutex);
    return m_sharedContext;
}

QnAbstractArchiveDelegate* HanwhaResource::createArchiveDelegate()
{
    if (isNvr())
        return new HanwhaArchiveDelegate(toSharedPointer().dynamicCast<HanwhaResource>());

    return nullptr;
}

void HanwhaResource::setSupportedAnalyticsEventTypeIds(
    QnUuid engineId, QSet<QString> supportedEvents)
{
    QSet<QString> externalEvents;
    for (const auto& eventTypeId: supportedEvents)
    {
        if (eventTypeId != kHanwhaInputPortEventTypeId)
            externalEvents.insert(eventTypeId);
    }

    base_type::setSupportedAnalyticsEventTypeIds(engineId, externalEvents);
}

QnTimePeriodList HanwhaResource::getDtsTimePeriods(
    qint64 startTimeMs,
    qint64 endTimeMs,
    int detailLevel,
    bool keepSmalChunks,
    int limit,
    Qt::SortOrder sortOrder)
{
    if (!isNvr())
        return QnTimePeriodList();

    const auto& timeline = sharedContext()->overlappedTimeline(getChannel());
    const auto numberOfOverlappedIds = timeline.size();
    NX_ASSERT(numberOfOverlappedIds <= 1, lit("There should be only one overlapped ID for NVR"));
    if (numberOfOverlappedIds != 1)
        return QnTimePeriodList();

    const auto& periods = timeline.cbegin()->second;
    auto itr = std::lower_bound(periods.begin(), periods.end(), startTimeMs);
    if (itr != periods.begin())
    {
        --itr;
        if (itr->endTimeMs() <= startTimeMs)
            ++itr; //< Case if previous chunk does not contain startTime.
    }
    auto endItr = std::lower_bound(periods.begin(), periods.end(), endTimeMs);

    return QnTimePeriodList::filterTimePeriods(
        itr, endItr, detailLevel, keepSmalChunks, limit, sortOrder);
}

QnConstResourceAudioLayoutPtr HanwhaResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    auto defaultLayout = nx::vms::server::resource::Camera::getAudioLayout(dataProvider);
    if (!isAudioEnabled())
        return defaultLayout;

    const auto reader = dynamic_cast<const HanwhaStreamReader*>(dataProvider);
    if (!reader)
        return defaultLayout;

    const auto layout = reader->getDPAudioLayout();
    if (layout)
        return layout;

    return defaultLayout;
}

bool HanwhaResource::setCameraCredentialsSync(const QAuthenticator& auth, QString* outErrorString)
{
    HanwhaRequestHelper helper(sharedContext());
    auto response = helper.view(lit("security/users"));
    if (!response.isSuccessful())
    {
        if (outErrorString)
            *outErrorString = response.errorString();
        return false;
    }

    QString userIndex;
    const auto data = response.response();
    for (auto itr = data.begin(); itr != data.end(); ++itr)
    {
        // Line example: Users.0=admin/Samsung2/True/True//True//True
        if (itr->second.split('/')[0] == auth.user())
        {
            userIndex = itr->first.split('.').last();
            break;
        }
    }
    if (userIndex.isEmpty())
    {
        if (outErrorString)
            *outErrorString = lm("User %1 not found").arg(auth.user());
        return false;
    }

    std::map<QString, QString> params;
    params.emplace(lit("UserID"), auth.user());
    params.emplace(lit("Password"), auth.password());
    params.emplace(lit("Index"), userIndex);
    response = helper.update(lit("security/users"), params);
    if (!response.isSuccessful())
    {
        if (outErrorString)
            *outErrorString = response.errorString();
        return false;
    }

    return true;
}

bool HanwhaResource::isConnectedViaSunapi() const
{
    return m_isChannelConnectedViaSunapi;
}

HanwhaProfileParameters HanwhaResource::makeProfileParameters(
    Qn::ConnectionRole role,
    const QnLiveStreamParams& parameters,
    HanwhaProfileParameterFlags flags) const
{
    NX_ASSERT(isConnectedViaSunapi());
    if (!isConnectedViaSunapi())
        return {};

    const auto codec = streamCodec(role);
    const auto codecProfile = streamCodecProfile(codec, role);
    const auto resolution = streamResolution(role);
    const auto frameRate = streamFrameRate(role, parameters.fps);
    const auto govLength = streamGovLength(role);
    const auto bitrateControl = streamBitrateControl(role); //< cbr/vbr
    const auto bitrate = streamBitrate(role, parameters);

    const auto govLengthParameterName =
        lit("%1.GOVLength").arg(toHanwhaString(codec));

    const auto bitrateControlParameterName =
        lit("%1.BitrateControlType").arg(toHanwhaString(codec));

    const bool isH26x = codec == AVCodecID::AV_CODEC_ID_H264
        || codec == AVCodecID::AV_CODEC_ID_HEVC;

    HanwhaProfileParameters result = {
        {kHanwhaChannelProperty, QString::number(getChannel())},
        {kHanwhaEncodingTypeProperty, toHanwhaString(codec)},
        {kHanwhaResolutionProperty, toHanwhaString(resolution)}
    };

    if (flags.testFlag(HanwhaProfileParameterFlag::newProfile))
        result.emplace(kHanwhaProfileNameProperty, nxProfileName(role));
    else
        result.emplace(kHanwhaProfileNumberProperty, QString::number(profileByRole(role)));

    const auto audioInputEnableParameter = cgiParameters().parameter(
        QString("media/videoprofile/add_update/") + kHanwhaAudioInputEnableProperty);

    if (flags.testFlag(HanwhaProfileParameterFlag::audioSupported) && audioInputEnableParameter)
        result.emplace(kHanwhaAudioInputEnableProperty, toHanwhaString(isAudioEnabled()));

    if (isH26x)
    {
        if (govLength != kHanwhaInvalidGovLength)
            result.emplace(govLengthParameterName, QString::number(govLength));
        if (!codecProfile.isEmpty())
            result.emplace(lit("%1.Profile").arg(toHanwhaString(codec)), codecProfile);
    }

    if (isH26x && bitrateControl != Qn::BitrateControl::undefined)
        result.emplace(bitrateControlParameterName, toHanwhaString(bitrateControl));

    if (bitrate != kHanwhaInvalidBitrate)
        result.emplace(kHanwhaBitrateProperty, QString::number(bitrate));

    if (frameRate != kHanwhaInvalidFps)
        result.emplace(kHanwhaFrameRatePriority, QString::number(frameRate));

    return result;
}

QString HanwhaResource::proxiedId() const
{
    return getProperty(kHanwhaProxiedIdParamName);
}

void HanwhaResource::setProxiedId(const QString& proxiedId)
{
    setProperty(kHanwhaProxiedIdParamName, proxiedId);
}

bool HanwhaResource::isBypassSupported() const
{
    // temporarily disable bypass for proxied multisensor cameras since we don't have
    // a reliable way to figure out NVR -> Camera channel mapping.
    return m_isBypassSupported
        && isNvr()
        && isConnectedViaSunapi()
        && !isProxiedMultisensorCamera();
}

bool HanwhaResource::isProxiedMultisensorCamera() const
{
    return m_proxiedDeviceChannelCount > 1;
}

void HanwhaResource::setDirectProfile(Qn::ConnectionRole role, int profileNumber)
{
    m_profileByRole[role].directNumber = profileNumber;
}

void HanwhaResource::setBypassProfile(Qn::ConnectionRole role, int profileNumber)
{
    m_profileByRole[role].bypassNumber = profileNumber;
}

Ptz::Capabilities HanwhaResource::ptzCapabilities(nx::core::ptz::Type ptzType) const
{
    const auto itr = m_ptzCapabilities.find(ptzType);
    if (itr == m_ptzCapabilities.cend())
        return Ptz::NoPtzCapabilities;

    return itr->second;
}

void HanwhaResource::setPtzCalibarionTimer()
{
    const auto kUpdateTimeout = sharedContext()->ptzCalibratedChannels.timeout() / 3;
    NX_VERBOSE(this, "Set PTZ calibration timer");
    m_timerHolder.addTimer(
        lm("%1 PTZ calibration").args(this),
        [this]()
        {
            if (getStatus() != Qn::Online && getStatus() != Qn::Recording)
            {
                NX_DEBUG(this, "PTZ calibration timer is not needed any more");
                return;
            }

            if (const auto calibratedChannels = sharedContext()->ptzCalibratedChannels())
            {
                const bool isRedirected = !getProperty(ResourcePropertyKey::kPtzTargetId).isEmpty();
                const bool isCalibrated = calibratedChannels->count(getChannel());
                if (isRedirected != isCalibrated)
                {
                    NX_DEBUG(this, "PTZ calibration has changed, go offline for reinitialization");
                    return setStatus(Qn::Offline);
                }
            }

            setPtzCalibarionTimer();
        },
        kUpdateTimeout);
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
