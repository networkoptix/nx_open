#include <set>

#include "openssl/evp.h"

#include <QElapsedTimer>

#include "soap_wrapper.h"
#include <onvif/Onvif.nsmap>

#include <nx/utils/log/log.h>
#include <QtCore/QtGlobal>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include "core/resource/resource.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include "media_server/settings.h"
#include <media_server/media_server_module.h>

using nx::vms::common::Credentials;

const QLatin1String DEFAULT_ONVIF_LOGIN = QLatin1String("admin");
const QLatin1String DEFAULT_ONVIF_PASSWORD = QLatin1String("admin");

namespace {

static PasswordHelper passwordHelper;

} // namespace

// ------------------------------------------------------------------------------------------------
// SoapTimeouts
// ------------------------------------------------------------------------------------------------

SoapTimeouts SoapTimeouts::minivalValue()
{
    using namespace std::chrono;
    SoapTimeouts result;
    result.sendTimeout = 1s;
    result.recvTimeout = 1s;
    result.connectTimeout = 1s;
    result.acceptTimeout = 1s;
    return result;
}

SoapTimeouts::SoapTimeouts(const QString& serialized)
{
    if (serialized.isEmpty())
        return;

    static const int kTimeoutsCount = 4;

    bool success = false;
    auto timeouts = serialized.split(';');
    auto paramsNum = timeouts.size();

    std::chrono::seconds* fieldsToSet[] =
        { &sendTimeout, &recvTimeout, &connectTimeout, &acceptTimeout };

    if (paramsNum == 1)
    {
        auto timeout = timeouts[0].toInt(&success);
        if (!success)
            return;
        for (auto i = 0; i < kTimeoutsCount; ++i)
            *(fieldsToSet[i]) = std::chrono::seconds(timeout);
    }
    else if (paramsNum == 4)
    {
        for (auto i = 0; i < kTimeoutsCount; ++i)
        {
            auto timeout = timeouts[i].toInt(&success);
            if (!success)
                continue;
            *(fieldsToSet[i]) = std::chrono::seconds(timeout);
        }
    }
}

QString SoapTimeouts::serialize() const
{
    return lit("%1;%2;%3;%4")
        .arg(std::chrono::seconds(sendTimeout).count())
        .arg(std::chrono::seconds(recvTimeout).count())
        .arg(std::chrono::seconds(connectTimeout).count())
        .arg(std::chrono::seconds(acceptTimeout).count());
}

void SoapTimeouts::assignTo(struct soap* soap) const
{
    soap->send_timeout = std::chrono::seconds(sendTimeout).count();
    soap->recv_timeout = std::chrono::seconds(recvTimeout).count();
    soap->connect_timeout = std::chrono::seconds(connectTimeout).count();
    soap->accept_timeout = std::chrono::seconds(acceptTimeout).count();
}
/**
 Generate code that initializes static constant members "kRequestFunc" and "kFuncName".
 The example of generated code:
const RequestTraits<_onvifDeviceIO__GetDigitalInputs, _onvifDeviceIO__GetDigitalInputsResponse>::RequestFunc
    RequestTraits<_onvifDeviceIO__GetDigitalInputsResponse>::kRequestFunc
    = &DeviceIOBindingProxy::GetDigitalInputs;
    const char RequestTraits<_onvifDeviceIO__GetDigitalInputsResponse>::kFuncName[64] = "GetDigitalInputs";
 */
#define NX_DEFINE_RESPONSE_TRAITS(WEBSERVICE, FUNCTION) \
    const RequestTraits<NX_MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION), NX_MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION)>::RequestFunc \
        RequestTraits<NX_MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION), NX_MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION)>::kRequestFunc = \
        &NX_MAKE_BINDINGPROXY_LEXEME(WEBSERVICE)::FUNCTION; \
    \
    const char RequestTraits<NX_MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION), NX_MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION)>::kFuncName[64] = \
        #FUNCTION;

#define NX_DEFINE_RESPONSE_TRAITS_IRREGULAR(WEBSERVICE, FUNCTION, REQUEST, RESPONSE) \
    const RequestTraits<REQUEST, RESPONSE>::RequestFunc \
        RequestTraits<REQUEST, RESPONSE>::kRequestFunc = \
        &NX_MAKE_BINDINGPROXY_LEXEME(WEBSERVICE)::FUNCTION; \
    \
    const char RequestTraits<REQUEST, RESPONSE>::kFuncName[64] = \
        #FUNCTION;

NX_DEFINE_RESPONSE_TRAITS(DeviceIO, GetDigitalInputs)
NX_DEFINE_RESPONSE_TRAITS_IRREGULAR(DeviceIO, GetRelayOutputs, _onvifDevice__GetRelayOutputs, _onvifDevice__GetRelayOutputsResponse)
NX_DEFINE_RESPONSE_TRAITS(DeviceIO, SetRelayOutputSettings)

NX_DEFINE_RESPONSE_TRAITS(Media, GetVideoEncoderConfiguration)
NX_DEFINE_RESPONSE_TRAITS(Media, GetVideoEncoderConfigurations)
NX_DEFINE_RESPONSE_TRAITS(Media, GetVideoEncoderConfigurationOptions)
NX_DEFINE_RESPONSE_TRAITS(Media, SetVideoEncoderConfiguration)
NX_DEFINE_RESPONSE_TRAITS(Media, GetAudioEncoderConfigurations)
NX_DEFINE_RESPONSE_TRAITS(Media, SetAudioEncoderConfiguration)
NX_DEFINE_RESPONSE_TRAITS(Media, GetProfiles)
NX_DEFINE_RESPONSE_TRAITS(Media, CreateProfile)

NX_DEFINE_RESPONSE_TRAITS_IRREGULAR(Media2, GetVideoEncoderConfigurations,
    onvifMedia2__GetConfiguration, _onvifMedia2__GetVideoEncoderConfigurationsResponse)
NX_DEFINE_RESPONSE_TRAITS_IRREGULAR(Media2, GetVideoEncoderConfigurationOptions,
    onvifMedia2__GetConfiguration, _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse)
NX_DEFINE_RESPONSE_TRAITS_IRREGULAR(Media2, SetVideoEncoderConfiguration,
    _onvifMedia2__SetVideoEncoderConfiguration, onvifMedia2__SetConfigurationResponse)
NX_DEFINE_RESPONSE_TRAITS(Media2, GetProfiles)
NX_DEFINE_RESPONSE_TRAITS(Media2, CreateProfile)
// ------------------------------------------------------------------------------------------------
// DeviceSoapWrapper
// ------------------------------------------------------------------------------------------------
DeviceSoapWrapper::DeviceSoapWrapper(
    const SoapTimeouts& timeouts,
    const std::string& endpoint,
    const QString& login,
    const QString& passwd,
    int timeDrift,
    bool tcpKeepAlive)
    :
    SoapWrapper<DeviceBindingProxy>(timeouts, endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

DeviceSoapWrapper::DeviceSoapWrapper(SoapParams soapParams) :
    SoapWrapper<DeviceBindingProxy>(std::move(soapParams))
{
}

DeviceSoapWrapper::~DeviceSoapWrapper()
{
}

void DeviceSoapWrapper::calcTimeDrift()
{
    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = GetSystemDateAndTime(request, response);

    if (soapRes == SOAP_OK && response.SystemDateAndTime && response.SystemDateAndTime->UTCDateTime)
    {
        onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
        onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;

        QDateTime datetime(QDate(date->Year, date->Month, date->Day), QTime(time->Hour, time->Minute, time->Second), Qt::UTC);
        m_timeDrift = datetime.toMSecsSinceEpoch() / 1000 - QDateTime::currentMSecsSinceEpoch() / 1000;
    }
}

QAuthenticator DeviceSoapWrapper::getDefaultPassword(
    QnCommonModule* commonModule,
    const QString& manufacturer, const QString& model) const
{
    QAuthenticator result;

    QnResourceData resourceData = commonModule->resourceDataPool()->data(manufacturer, model);
    QString credentials = resourceData.value<QString>(lit("defaultCredentials"));
    QStringList parts = credentials.split(L':');
    if (parts.size() == 2) {
        result.setUser(parts[0]);
        result.setPassword(parts[1]);
    }

    return result;
}

std::list<nx::vms::common::Credentials> DeviceSoapWrapper::getPossibleCredentials(
    QnCommonModule* commonModule,
    const QString& manufacturer,
    const QString& model) const
{
    QnResourceData resData = commonModule->resourceDataPool()->data(manufacturer, model);
    auto credentials = resData.value<QList<nx::vms::common::Credentials>>(
        ResourceDataKey::kPossibleDefaultCredentials);

    return credentials.toStdList();
}

nx::vms::common::Credentials DeviceSoapWrapper::getForcedCredentials(
    QnCommonModule* commonModule,
    const QString& manufacturer,
    const QString& model)
{
    QnResourceData resData = commonModule->resourceDataPool()->data(manufacturer, model);
    auto credentials = resData.value<nx::vms::common::Credentials>(
        ResourceDataKey::kForcedDefaultCredentials);

    return credentials;
}

bool DeviceSoapWrapper::fetchLoginPassword(
    QnCommonModule* commonModule,
    const QString& manufacturer, const QString& model)
{
    auto forcedCredentials = getForcedCredentials(commonModule, manufacturer, model);

    if (!forcedCredentials.user.isEmpty())
    {
        setLogin(forcedCredentials.user);
        setPassword(forcedCredentials.password);
        return true;
    }

    const auto oldCredentials =
        passwordHelper.getCredentialsByManufacturer(manufacturer);

    auto possibleCredentials = oldCredentials;

    const auto credentialsFromResourceData = getPossibleCredentials(commonModule, manufacturer, model);
    for (const auto& credentials: credentialsFromResourceData)
    {
        if (!oldCredentials.contains(credentials))
            possibleCredentials.append(credentials);
    }

    QnResourceData resData = commonModule->resourceDataPool()->data(manufacturer, model);
    auto timeoutSec = resData.value<int>(ResourceDataKey::kUnauthorizedTimeoutSec);
    if (timeoutSec > 0 && possibleCredentials.size() > 1)
    {
        possibleCredentials.erase(possibleCredentials.begin() + 1, possibleCredentials.end());
        NX_WARNING(this,
            lm("strict credentials list for camera %1 to 1 record. because of non zero '%2' parameter.").
            arg(endpoint()).
            arg(ResourceDataKey::kUnauthorizedTimeoutSec));
    }

    if (possibleCredentials.size() <= 1)
    {
        nx::vms::common::Credentials credentials;
        if (!possibleCredentials.isEmpty())
            credentials = possibleCredentials.first();
        setLogin(credentials.user);
        setPassword(credentials.password);
        return true;
    }

    // Start logging timeouts as soon as network requests appear.
    QElapsedTimer timer;
    timer.restart();
    auto logTimeout = [&](bool found)
    {
        NX_DEBUG(this, lit("autodetect credentials for camera %1 took %2 ms. Credentials found: %3").
            arg(endpoint()).
            arg(timer.elapsed()).
            arg(found));
    };

    calcTimeDrift();
    for (const auto& credentials : possibleCredentials)
    {
        if (commonModule->isNeedToStop())
            return false;

        setLogin(credentials.user);
        setPassword(credentials.password);

        NetIfacesReq request;
        NetIfacesResp response;
        auto soapRes = getNetworkInterfaces(request, response);

        if (soapRes == SOAP_OK || !lastErrorIsNotAuthenticated())
        {
            logTimeout(soapRes == SOAP_OK);
            return soapRes == SOAP_OK;
        }
    }
    logTimeout(false);

    const auto& credentials = possibleCredentials.first();
    setLogin(credentials.user);
    setPassword(credentials.password);

    return false;
}

int DeviceSoapWrapper::getNetworkInterfaces(NetIfacesReq& request, NetIfacesResp& response)
{
    beforeMethodInvocation<NetIfacesReq>();
    return m_bindingProxy.GetNetworkInterfaces(m_endpoint, NULL, &request, response);
}

int DeviceSoapWrapper::createUsers(CreateUsersReq& request, CreateUsersResp& response)
{
    beforeMethodInvocation<CreateUsersReq>();
    return m_bindingProxy.CreateUsers(m_endpoint, NULL, &request, response);
}

int DeviceSoapWrapper::getDeviceInformation(DeviceInfoReq& request, DeviceInfoResp& response)
{
    beforeMethodInvocation<DeviceInfoReq>();
    return m_bindingProxy.GetDeviceInformation(m_endpoint, NULL, &request, response);
}

int DeviceSoapWrapper::getServiceCapabilities(_onvifDevice__GetServiceCapabilities& request, _onvifDevice__GetServiceCapabilitiesResponse& response)
{
    return invokeMethod(&DeviceBindingProxy::GetServiceCapabilities, &request, response);
}

int DeviceSoapWrapper::getRelayOutputs(_onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response)
{
    return invokeMethod(&DeviceBindingProxy::GetRelayOutputs, &request, response);
}

int DeviceSoapWrapper::setRelayOutputState(_onvifDevice__SetRelayOutputState& request, _onvifDevice__SetRelayOutputStateResponse& response)
{
    return invokeMethod(&DeviceBindingProxy::SetRelayOutputState, &request, response);
}

int DeviceSoapWrapper::setRelayOutputSettings(_onvifDevice__SetRelayOutputSettings& request, _onvifDevice__SetRelayOutputSettingsResponse& response)
{
    return invokeMethod(&DeviceBindingProxy::SetRelayOutputSettings, &request, response);
}

int DeviceSoapWrapper::getCapabilities(_onvifDevice__GetCapabilities& request, _onvifDevice__GetCapabilitiesResponse& response)
{
    return invokeMethod(&DeviceBindingProxy::GetCapabilities, &request, response);
}

int DeviceSoapWrapper::getServices(GetServicesReq& request, GetServicesResp& response)
{
    beforeMethodInvocation<GetServicesReq>();
    int rez = m_bindingProxy.GetServices(m_endpoint, NULL, &request, response);
    return rez;
}

int DeviceSoapWrapper::GetSystemDateAndTime(_onvifDevice__GetSystemDateAndTime& request, _onvifDevice__GetSystemDateAndTimeResponse& response)
{
    beforeMethodInvocation<_onvifDevice__GetSystemDateAndTime>();
    return m_bindingProxy.GetSystemDateAndTime(m_endpoint, NULL, &request, response);
}

int DeviceSoapWrapper::systemReboot(RebootReq& request, RebootResp& response)
{
    beforeMethodInvocation<RebootReq>();
    return m_bindingProxy.SystemReboot(m_endpoint, NULL, &request, response);
}

int DeviceSoapWrapper::setSystemFactoryDefaultHard(FactoryDefaultReq& request, FactoryDefaultResp& response)
{
    beforeMethodInvocation<FactoryDefaultReq>();
    request.FactoryDefault = onvifXsd__FactoryDefaultType::Hard;
    return m_bindingProxy.SetSystemFactoryDefault(m_endpoint, NULL, &request, response);
}

int DeviceSoapWrapper::setSystemFactoryDefaultSoft(FactoryDefaultReq& request, FactoryDefaultResp& response)
{
    beforeMethodInvocation<FactoryDefaultReq>();
    request.FactoryDefault = onvifXsd__FactoryDefaultType::Soft;
    return m_bindingProxy.SetSystemFactoryDefault(m_endpoint, NULL, &request, response);
}
// ------------------------------------------------------------------------------------------------
// MediaSoapWrapper
// ------------------------------------------------------------------------------------------------
int MediaSoapWrapper::getVideoEncoderConfigurationOptions(VideoOptionsReq& request, VideoOptionsResp& response)
{
    beforeMethodInvocation<VideoOptionsReq>();
    return m_bindingProxy.GetVideoEncoderConfigurationOptions(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getAudioOutputConfigurations(GetAudioOutputConfigurationsReq& request, GetAudioOutputConfigurationsResp& response)
{
    beforeMethodInvocation<GetAudioOutputConfigurationsReq>();
    return m_bindingProxy.GetAudioOutputConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addAudioOutputConfiguration(AddAudioOutputConfigurationReq& request, AddAudioOutputConfigurationResp& response)
{
    beforeMethodInvocation<AddAudioOutputConfigurationReq>();
    return m_bindingProxy.AddAudioOutputConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addAudioDecoderConfiguration(
    AddAudioDecoderConfigurationReq& request,
    AddAudioDecoderConfigurationResp& response)
{
    beforeMethodInvocation<AddAudioDecoderConfigurationReq>();
    return m_bindingProxy.AddAudioDecoderConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getCompatibleAudioDecoderConfigurations(
    GetCompatibleAudioDecoderConfigurationsReq& request,
    GetCompatibleAudioDecoderConfigurationsResp& response)
{
    beforeMethodInvocation<GetCompatibleAudioDecoderConfigurationsReq>();
    return m_bindingProxy.GetCompatibleAudioDecoderConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getAudioEncoderConfigurationOptions(AudioOptionsReq& request, AudioOptionsResp& response)
{
    beforeMethodInvocation<AudioOptionsReq>();
    return m_bindingProxy.GetAudioEncoderConfigurationOptions(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getVideoSourceConfigurations(VideoSrcConfigsReq& request, VideoSrcConfigsResp& response)
{
    beforeMethodInvocation<VideoSrcConfigsReq>();
    return m_bindingProxy.GetVideoSourceConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getAudioOutputs(_onvifMedia__GetAudioOutputs& request, _onvifMedia__GetAudioOutputsResponse& response)
{
    beforeMethodInvocation<_onvifMedia__GetAudioOutputs>();
    return m_bindingProxy.GetAudioOutputs(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getVideoSources(_onvifMedia__GetVideoSources& request, _onvifMedia__GetVideoSourcesResponse& response)
{
    beforeMethodInvocation<_onvifMedia__GetVideoSources>();
    return m_bindingProxy.GetVideoSources(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getCompatibleMetadataConfigurations(CompatibleMetadataConfiguration& request, CompatibleMetadataConfigurationResp& response)
{
    beforeMethodInvocation<CompatibleMetadataConfiguration>();
    return m_bindingProxy.GetCompatibleMetadataConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getVideoEncoderConfigurations(VideoConfigsReq& request, VideoConfigsResp& response)
{
    beforeMethodInvocation<VideoConfigsReq>();
    return m_bindingProxy.GetVideoEncoderConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getProfiles(ProfilesReq& request, ProfilesResp& response)
{
    beforeMethodInvocation<ProfilesReq>();
    return m_bindingProxy.GetProfiles(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addVideoSourceConfiguration(AddVideoSrcConfigReq& request, AddVideoSrcConfigResp& response)
{
    beforeMethodInvocation<AddVideoSrcConfigReq>();
    return m_bindingProxy.AddVideoSourceConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::createProfile(CreateProfileReq& request, CreateProfileResp& response)
{
    beforeMethodInvocation<CreateProfileReq>();
    return m_bindingProxy.CreateProfile(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addVideoEncoderConfiguration(AddVideoConfigReq& request, AddVideoConfigResp& response)
{
    beforeMethodInvocation<AddVideoConfigReq>();
    return m_bindingProxy.AddVideoEncoderConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addPTZConfiguration(AddPTZConfigReq& request, AddPTZConfigResp& response)
{
    beforeMethodInvocation<AddPTZConfigReq>();
    return m_bindingProxy.AddPTZConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::setVideoEncoderConfiguration(SetVideoConfigReq& request, SetVideoConfigResp& response)
{
    beforeMethodInvocation<SetVideoConfigReq>();
    return m_bindingProxy.SetVideoEncoderConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getProfile(ProfileReq& request, ProfileResp& response)
{
    beforeMethodInvocation<ProfileReq>();
    return m_bindingProxy.GetProfile(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getStreamUri(StreamUriReq& request, StreamUriResp& response)
{
    beforeMethodInvocation<StreamUriReq>();
    return m_bindingProxy.GetStreamUri(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::setVideoSourceConfiguration(SetVideoSrcConfigReq& request, SetVideoSrcConfigResp& response)
{
    beforeMethodInvocation<SetVideoSrcConfigReq>();
    return m_bindingProxy.SetVideoSourceConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getAudioEncoderConfigurations(AudioConfigsReq& request, AudioConfigsResp& response)
{
    beforeMethodInvocation<AudioConfigsReq>();
    return m_bindingProxy.GetAudioEncoderConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addAudioEncoderConfiguration(AddAudioConfigReq& request, AddAudioConfigResp& response)
{
    beforeMethodInvocation<AddAudioConfigReq>();
    return m_bindingProxy.AddAudioEncoderConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::setAudioEncoderConfiguration(SetAudioConfigReq& request, SetAudioConfigResp& response)
{
    beforeMethodInvocation<SetAudioConfigReq>();
    return m_bindingProxy.SetAudioEncoderConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getAudioSourceConfigurations(AudioSrcConfigsReq& request, AudioSrcConfigsResp& response)
{
    beforeMethodInvocation<AudioSrcConfigsReq>();
    return m_bindingProxy.GetAudioSourceConfigurations(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::setAudioSourceConfiguration(SetAudioSrcConfigReq& request, SetAudioSrcConfigResp& response)
{
    beforeMethodInvocation<SetAudioSrcConfigReq>();
    return m_bindingProxy.SetAudioSourceConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::addAudioSourceConfiguration(AddAudioSrcConfigReq& request, AddAudioSrcConfigResp& response)
{
    beforeMethodInvocation<AddAudioSrcConfigReq>();
    return m_bindingProxy.AddAudioSourceConfiguration(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getVideoSourceConfigurationOptions(VideoSrcOptionsReq& request, VideoSrcOptionsResp& response)
{
    beforeMethodInvocation<VideoSrcOptionsReq>();
    return m_bindingProxy.GetVideoSourceConfigurationOptions(m_endpoint, NULL, &request, response);
}

int MediaSoapWrapper::getVideoEncoderConfiguration(VideoConfigReq& request, VideoConfigResp& response)
{
    beforeMethodInvocation<VideoConfigReq>();
    return m_bindingProxy.GetVideoEncoderConfiguration(m_endpoint, NULL, &request, response);
}

// ------------------------------------------------------------------------------------------------
// ImagingSoapWrapper
// ------------------------------------------------------------------------------------------------
ImagingSoapWrapper::ImagingSoapWrapper(
    const SoapTimeouts& timeouts,
    const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive)
    :
    SoapWrapper<ImagingBindingProxy>(timeouts, endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

ImagingSoapWrapper::ImagingSoapWrapper(SoapParams soapParams) :
    SoapWrapper<ImagingBindingProxy>(std::move(soapParams))
{
}

ImagingSoapWrapper::~ImagingSoapWrapper()
{
}

int ImagingSoapWrapper::getOptions(ImagingOptionsReq& request, ImagingOptionsResp& response)
{
    beforeMethodInvocation<ImagingOptionsReq>();
    return m_bindingProxy.GetOptions(m_endpoint, NULL, &request, response);
}

int ImagingSoapWrapper::getImagingSettings(ImagingSettingsReq& request, ImagingSettingsResp& response)
{
    beforeMethodInvocation<ImagingSettingsReq>();
    return m_bindingProxy.GetImagingSettings(m_endpoint, NULL, &request, response);
}

int ImagingSoapWrapper::setImagingSettings(SetImagingSettingsReq& request, SetImagingSettingsResp& response)
{
    beforeMethodInvocation<SetImagingSettingsReq>();
    return m_bindingProxy.SetImagingSettings(m_endpoint, NULL, &request, response);
}

int ImagingSoapWrapper::getMoveOptions(_onvifImg__GetMoveOptions &request, _onvifImg__GetMoveOptionsResponse& response)
{
    return invokeMethod(&ImagingBindingProxy::GetMoveOptions, &request, response);
}

int ImagingSoapWrapper::move(_onvifImg__Move &request, _onvifImg__MoveResponse& response)
{
    return invokeMethod(&ImagingBindingProxy::Move, &request, response);
}

// ------------------------------------------------------------------------------------------------
// PtzSoapWrapper
// ------------------------------------------------------------------------------------------------
int PtzSoapWrapper::doAbsoluteMove(AbsoluteMoveReq& request, AbsoluteMoveResp& response)
{
    beforeMethodInvocation<AbsoluteMoveReq>();
    return m_bindingProxy.AbsoluteMove(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::doRelativeMove(RelativeMoveReq& request, RelativeMoveResp& response)
{
    beforeMethodInvocation<RelativeMoveReq>();
    return m_bindingProxy.RelativeMove(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::gotoPreset(GotoPresetReq& request, GotoPresetResp& response)
{
    beforeMethodInvocation<GotoPresetReq>();
    return m_bindingProxy.GotoPreset(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::setPreset(SetPresetReq& request, SetPresetResp& response)
{
    beforeMethodInvocation<SetPresetReq>();
    return m_bindingProxy.SetPreset(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::getPresets(GetPresetsReq& request, GetPresetsResp& response)
{
    beforeMethodInvocation<GetPresetsReq>();
    return m_bindingProxy.GetPresets(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::removePreset(RemovePresetReq& request, RemovePresetResp& response)
{
    beforeMethodInvocation<RemovePresetReq>();
    return m_bindingProxy.RemovePreset(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::doGetNode(_onvifPtz__GetNode& request, _onvifPtz__GetNodeResponse& response)
{
    beforeMethodInvocation<_onvifPtz__GetNode>();
    int rez = m_bindingProxy.GetNode(m_endpoint, NULL, &request, response);
    if (rez != SOAP_OK)
    {
        qWarning() << "PTZ settings reading error: " << endpoint() <<  ". " << getLastErrorDescription();
    }
    return rez;
}

int PtzSoapWrapper::doGetNodes(_onvifPtz__GetNodes& request, _onvifPtz__GetNodesResponse& response)
{
    beforeMethodInvocation<_onvifPtz__GetNodes>();
    int rez = m_bindingProxy.GetNodes(m_endpoint, NULL, &request, response);
    return rez;
}

int PtzSoapWrapper::doGetConfigurations(_onvifPtz__GetConfigurations& request, _onvifPtz__GetConfigurationsResponse& response)
{
    beforeMethodInvocation<_onvifPtz__GetConfigurations>();
    int rez = m_bindingProxy.GetConfigurations(m_endpoint, NULL, &request, response);
    return rez;
}

int PtzSoapWrapper::doContinuousMove(_onvifPtz__ContinuousMove& request, _onvifPtz__ContinuousMoveResponse& response)
{
    beforeMethodInvocation<_onvifPtz__ContinuousMove>();
    return m_bindingProxy.ContinuousMove(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::doGetStatus(_onvifPtz__GetStatus& request, _onvifPtz__GetStatusResponse& response)
{
    beforeMethodInvocation<_onvifPtz__GetStatus>();
    return m_bindingProxy.GetStatus(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::doStop(_onvifPtz__Stop& request, _onvifPtz__StopResponse& response)
{
    beforeMethodInvocation<_onvifPtz__Stop>();
    return m_bindingProxy.Stop(m_endpoint, NULL, &request, response);
}

int PtzSoapWrapper::doGetServiceCapabilities(PtzGetServiceCapabilitiesReq& request, PtzPtzGetServiceCapabilitiesResp& response)
{
    beforeMethodInvocation<PtzGetServiceCapabilitiesReq>();
    int rez = m_bindingProxy.GetServiceCapabilities(m_endpoint, NULL, &request, response);
    return rez;
}

// ------------------------------------------------------------------------------------------------
// NotificationProducerSoapWrapper
// ------------------------------------------------------------------------------------------------
NotificationProducerSoapWrapper::NotificationProducerSoapWrapper(
    const SoapTimeouts& timeouts,
    const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive)
    :
    SoapWrapper<NotificationProducerBindingProxy>(timeouts, endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

NotificationProducerSoapWrapper::NotificationProducerSoapWrapper(SoapParams soapParams) :
    SoapWrapper<NotificationProducerBindingProxy>(std::move(soapParams))
{
}

int NotificationProducerSoapWrapper::subscribe(_oasisWsnB2__Subscribe* const request, _oasisWsnB2__SubscribeResponse* const response)
{
    return invokeMethod(&NotificationProducerBindingProxy::Subscribe, request, *response);
}

// ------------------------------------------------------------------------------------------------
// EventSoapWrapper
// ------------------------------------------------------------------------------------------------
int EventSoapWrapper::createPullPointSubscription(
    _onvifEvents__CreatePullPointSubscription& request,
    _onvifEvents__CreatePullPointSubscriptionResponse& response)
{
    return invokeMethod(&EventBindingProxy::CreatePullPointSubscription, &request, response);
}
// ------------------------------------------------------------------------------------------------
// CreatePullPointSoapWrapper
// ------------------------------------------------------------------------------------------------
PullPointSubscriptionWrapper::PullPointSubscriptionWrapper(
    const SoapTimeouts& timeouts,
    const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive)
    :
    SoapWrapper<PullPointSubscriptionBindingProxy>(timeouts, endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

PullPointSubscriptionWrapper::PullPointSubscriptionWrapper(SoapParams soapParams) :
    SoapWrapper<PullPointSubscriptionBindingProxy>(std::move(soapParams))
{
}

int PullPointSubscriptionWrapper::pullMessages(_onvifEvents__PullMessages& request, _onvifEvents__PullMessagesResponse& response)
{
    return invokeMethod(&PullPointSubscriptionBindingProxy::PullMessages, &request, response);
}
// ------------------------------------------------------------------------------------------------
// SubscriptionManagerSoapWrapper
// ------------------------------------------------------------------------------------------------
int SubscriptionManagerSoapWrapper::renew(_oasisWsnB2__Renew& request, _oasisWsnB2__RenewResponse& response)
{
    return invokeMethod(&SubscriptionManagerBindingProxy::Renew, &request, response);
}

int SubscriptionManagerSoapWrapper::unsubscribe(_oasisWsnB2__Unsubscribe& request, _oasisWsnB2__UnsubscribeResponse& response)
{
    return invokeMethod(&SubscriptionManagerBindingProxy::Unsubscribe, &request, response);
}
// ------------------------------------------------------------------------------------------------
