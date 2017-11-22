#ifdef ENABLE_ONVIF

#include <algorithm>
#include <climits>
#include <cstring>
#include <cmath>
#include <sstream>
#include <type_traits>

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QHash>
#include <QtCore/QTimer>

#include <onvif/soapDeviceBindingProxy.h>
#include <onvif/soapMediaBindingProxy.h>
#include <onvif/soapNotificationProducerBindingProxy.h>
#include <onvif/soapEventBindingProxy.h>
#include <onvif/soapPullPointSubscriptionBindingProxy.h>
#include <onvif/soapSubscriptionManagerBindingProxy.h>
#include <onvif/soapStub.h>

#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "onvif_helper.h"
#include <nx/utils/log/log.h>
#include "utils/common/synctime.h"
#include "utils/math/math.h"
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/system_error.h>
#include "api/app_server_connection.h"
#include "soap/soapserver.h"
#include "onvif_ptz_controller.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <nx/utils/timer_manager.h>
#include "gsoap_async_call_wrapper.h"
#include "plugins/resource/d-link/dlink_ptz_controller.h"
#include "core/onvif/onvif_config_data.h"

#include <plugins/resource/onvif/imaging/onvif_imaging_proxy.h>
#include <plugins/resource/onvif/onvif_maintenance_proxy.h>

#include <nx/fusion/model_functions.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <core/resource/resource_data_structures.h>

#include <plugins/utils/multisensor_data_provider.h>
#include <core/resource_management/resource_properties.h>
#include <common/static_common_module.h>
#include <core/dataconsumer/basic_audio_transmitter.h>

//!assumes that camera can only work in bistable mode (true for some (or all?) DW cameras)
#define SIMULATE_RELAY_PORT_MOMOSTABLE_MODE

namespace
{
    const QString kBaselineH264Profile("Baseline");
    const QString kMainH264Profile("Main");
    const QString kExtendedH264Profile("Extended");
    const QString kHighH264Profile("High");

    onvifXsd__H264Profile fromStringToH264Profile(const QString& str)
    {
        if (str == kMainH264Profile)
            return onvifXsd__H264Profile::onvifXsd__H264Profile__Main;
        else if (str == kExtendedH264Profile)
            return onvifXsd__H264Profile::onvifXsd__H264Profile__Extended;
        else if (str == kHighH264Profile)
            return onvifXsd__H264Profile::onvifXsd__H264Profile__High;
        else
            return onvifXsd__H264Profile::onvifXsd__H264Profile__Baseline;
    };
}


const QString QnPlOnvifResource::MANUFACTURE(lit("OnvifDevice"));
static const float MAX_EPS = 0.01f;
//static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifResource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifResource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifResource::DEFAULT_IFRAME_DISTANCE = 20;
QString QnPlOnvifResource::MEDIA_URL_PARAM_NAME = QLatin1String("MediaUrl");
QString QnPlOnvifResource::ONVIF_URL_PARAM_NAME = QLatin1String("DeviceUrl");
QString QnPlOnvifResource::ONVIF_ID_PARAM_NAME = QLatin1String("DeviceID");
const float QnPlOnvifResource::QUALITY_COEF = 0.2f;
const int QnPlOnvifResource::MAX_AUDIO_BITRATE = 64; //kbps
const int QnPlOnvifResource::MAX_AUDIO_SAMPLERATE = 32; //khz
const int QnPlOnvifResource::ADVANCED_SETTINGS_VALID_TIME = 60; //60s
static const unsigned int DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT = 30;
//!if renew subscription exactly at termination time, camera can already terminate subscription, so have to do that a little bit earlier..
static const unsigned int RENEW_NOTIFICATION_FORWARDING_SECS = 5;
static const unsigned int MS_PER_SECOND = 1000;
static const unsigned int PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC = 1;
static const unsigned int MAX_IO_PORTS_PER_DEVICE = 200;
static const int DEFAULT_SOAP_TIMEOUT = 10;
static const quint32 MAX_TIME_DRIFT_UPDATE_PERIOD_MS = 15 * 60 * 1000; // 15 minutes

//Forth times greater than default = 320 x 240

/* Some cameras declare invalid max resolution */
struct StrictResolution {
    const char* model;
    QSize maxRes;
};

// strict maximum resolution for this models

// TODO: #Elric #VASILENKO move out to JSON
StrictResolution strictResolutionList[] =
{
    { "Brickcom-30xN", QSize(1920, 1080) }
};

//width > height is preferred
static bool resolutionGreaterThan(const QSize &s1, const QSize &s2)
{
    long long res1 = s1.width() * s1.height();
    long long res2 = s2.width() * s2.height();
    return res1 > res2? true: (res1 == res2 && s1.width() > s2.width()? true: false);
}

class VideoOptionsLocal
{
public:
    VideoOptionsLocal(): isH264(false), minQ(-1), maxQ(-1), frameRateMin(-1), frameRateMax(-1), govMin(-1), govMax(-1), usedInProfiles(false) {}
    VideoOptionsLocal(const QString& _id, const VideoOptionsResp& resp, bool isH264Allowed, QnBounds frameRateBounds = QnBounds())
    {
        usedInProfiles = false;
        id = _id;

        std::vector<onvifXsd__VideoResolution*>* srcVector = 0;
        if (resp.Options->H264)
            srcVector = &resp.Options->H264->ResolutionsAvailable;
        else if (resp.Options->JPEG)
            srcVector = &resp.Options->JPEG->ResolutionsAvailable;
        if (srcVector) {
            for (uint i = 0; i < srcVector->size(); ++i)
                resolutions << QSize(srcVector->at(i)->Width, srcVector->at(i)->Height);
        }
        isH264 = resp.Options->H264 && isH264Allowed;
        if (isH264) {
            for (uint i = 0; i < resp.Options->H264->H264ProfilesSupported.size(); ++i)
                h264Profiles << resp.Options->H264->H264ProfilesSupported[i];
            std::sort(h264Profiles.begin(), h264Profiles.end());

            if (resp.Options->H264->FrameRateRange)
            {
                frameRateMax = restrictFrameRate(resp.Options->H264->FrameRateRange->Max, frameRateBounds);
                frameRateMin = restrictFrameRate(resp.Options->H264->FrameRateRange->Min, frameRateBounds);
            }

            if (resp.Options->H264->GovLengthRange) {
                govMin = resp.Options->H264->GovLengthRange->Min;
                govMax = resp.Options->H264->GovLengthRange->Max;
            }
        }
        else if (resp.Options->JPEG) {
            if (resp.Options->JPEG->FrameRateRange)
            {
                frameRateMax = restrictFrameRate(resp.Options->JPEG->FrameRateRange->Max, frameRateBounds);
                frameRateMin = restrictFrameRate(resp.Options->JPEG->FrameRateRange->Min, frameRateBounds);
            }
        }
        if (resp.Options->QualityRange) {
            minQ = resp.Options->QualityRange->Min;
            maxQ = resp.Options->QualityRange->Max;
        }
    }
    QVector<onvifXsd__H264Profile> h264Profiles;
    QString id;
    QList<QSize> resolutions;
    bool isH264;
    int minQ;
    int maxQ;
    int frameRateMin;
    int frameRateMax;
    int govMin;
    int govMax;
    bool usedInProfiles;
    QString currentProfile;

private:
    int restrictFrameRate(int frameRate, QnBounds frameRateBounds) const
    {
        if (frameRateBounds.isNull())
            return frameRate;

        return qBound((int)frameRateBounds.min, frameRate, (int)frameRateBounds.max);
    }
};

typedef std::function<bool(const VideoOptionsLocal&, const VideoOptionsLocal&)> VideoOptionsComparator;

bool videoOptsGreaterThan(const VideoOptionsLocal &s1, const VideoOptionsLocal &s2)
{
    int square1Max = 0;
    QSize max1Res;
    for (int i = 0; i < s1.resolutions.size(); ++i) {
        int newMax = s1.resolutions[i].width() * s1.resolutions[i].height();
        if (newMax > square1Max) {
            square1Max = newMax;
            max1Res = s1.resolutions[i];
        }
    }

    int square2Max = 0;
    QSize max2Res;
    for (int i = 0; i < s2.resolutions.size(); ++i) {
        int newMax = s2.resolutions[i].width() * s2.resolutions[i].height();
        if (newMax > square2Max) {
            square2Max = newMax;
            max2Res = s2.resolutions[i];
        }
    }

    if (square1Max != square2Max)
        return square1Max > square2Max;

    // if some option doesn't have H264 it "less"
    if (!s1.isH264 && s2.isH264)
        return false;
    else if (s1.isH264 && !s2.isH264)
        return true;

    if (!s1.usedInProfiles && s2.usedInProfiles)
        return false;
    else if (s1.usedInProfiles && !s2.usedInProfiles)
        return true;

    return s1.id < s2.id; // sort by name
}

bool compareByProfiles(const VideoOptionsLocal &s1, const VideoOptionsLocal &s2, const QMap<QString, int>& profilePriorities)
{
    auto firstPriority = profilePriorities.contains(s1.currentProfile)
        ? profilePriorities[s1.currentProfile]
        : -1;

    auto secondPriority = profilePriorities.contains(s2.currentProfile)
        ? profilePriorities[s2.currentProfile]
        : -1;

    if (firstPriority != secondPriority)
        return firstPriority > secondPriority;

    return videoOptsGreaterThan(s1, s2);
}

VideoOptionsComparator createComparator(const QString& profiles)
{
    if (!profiles.isEmpty())
    {
        auto profileList = profiles.split(L',');
        QMap<QString, int> profilePriorities;
        for (auto i = 0; i < profileList.size(); ++i)
            profilePriorities[profileList[i]] = profileList.size() - i;

        return
            [profilePriorities](const VideoOptionsLocal &s1, const VideoOptionsLocal &s2) -> bool
            {
                return compareByProfiles(s1, s2, profilePriorities);
            };
    }

    return videoOptsGreaterThan;
}

static void updateTimer(nx::utils::TimerId* timerId, std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void(nx::utils::TimerId)> function)
{
    if (*timerId != 0)
    {
        nx::utils::TimerManager::instance()->deleteTimer(*timerId);
        *timerId = 0;
    }

    *timerId = nx::utils::TimerManager::instance()->addTimer(
        std::move(function), timeout);
}

//
// QnPlOnvifResource
//

QnPlOnvifResource::RelayOutputInfo::RelayOutputInfo()
:
    isBistable( false ),
    activeByDefault( false )
{
}

QnPlOnvifResource::RelayOutputInfo::RelayOutputInfo(
    const std::string& _token,
    bool _isBistable,
    const std::string& _delayTime,
    bool _activeByDefault )
:
    token( _token ),
    isBistable( _isBistable ),
    delayTime( _delayTime ),
    activeByDefault( _activeByDefault )
{
}

QnPlOnvifResource::QnPlOnvifResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_iframeDistance(-1),
    m_minQuality(0),
    m_maxQuality(0),
    m_primaryCodec(H264),
    m_secondaryCodec(H264),
    m_audioCodec(AUDIO_NONE),
    m_primaryResolution(EMPTY_RESOLUTION_PAIR),
    m_secondaryResolution(EMPTY_RESOLUTION_PAIR),
    m_primaryH264Profile(-1),
    m_secondaryH264Profile(-1),
    m_audioBitrate(0),
    m_audioSamplerate(0),
    m_timeDrift(0),
    m_isRelayOutputInversed(false),
    m_inputMonitored(false),
    m_clearInputsTimeoutUSec(0),
    m_eventMonitorType(emtNone),
    m_nextPullMessagesTimerID(0),
    m_renewSubscriptionTimerID(0),
    m_maxChannels(1),
    m_streamConfCounter(0),
    m_prevPullMessageResponseClock(0),
    m_inputPortCount(0),
    m_videoLayout(nullptr),
    m_onvifRecieveTimeout(DEFAULT_SOAP_TIMEOUT),
    m_onvifSendTimeout(DEFAULT_SOAP_TIMEOUT)
{
    m_monotonicClock.start();
    m_advSettingsLastUpdated.restart();
}

QnPlOnvifResource::~QnPlOnvifResource()
{
    {
        QnMutexLocker lk( &m_ioPortMutex );
        while( !m_triggerOutputTasks.empty() )
        {
            const quint64 timerID = m_triggerOutputTasks.begin()->first;
            const TriggerOutputTask outputTask = m_triggerOutputTasks.begin()->second;
            m_triggerOutputTasks.erase( m_triggerOutputTasks.begin() );

            lk.unlock();

            nx::utils::TimerManager::instance()->joinAndDeleteTimer( timerID );    //garantees that no onTimer(timerID) is running on return
            if( !outputTask.active )
            {
                //returning port to inactive state
                setRelayOutputStateNonSafe(
                    0,
                    outputTask.outputID,
                    outputTask.active,
                    0);
            }

            lk.relock();
        }
    }

    stopInputPortMonitoringAsync();

    QnMutexLocker lock(&m_physicalParamsMutex);
    m_imagingParamsProxy.reset();
}

const QString QnPlOnvifResource::fetchMacAddress(
    const NetIfacesResp& response,
    const QString& senderIpAddress)
{
    QString someMacAddress;
    std::vector<class onvifXsd__NetworkInterface*> ifaces = response.NetworkInterfaces;

    for (uint i = 0; i < ifaces.size(); ++i)
    {
        onvifXsd__NetworkInterface* ifacePtr = ifaces[i];

        if (!ifacePtr->Info)
            continue;

        if (ifacePtr->Enabled && ifacePtr->IPv4 && ifacePtr->IPv4->Enabled)
        {
            onvifXsd__IPv4Configuration* conf = ifacePtr->IPv4->Config;
            if (!conf)
                continue;

            if (conf->DHCP && conf->FromDHCP) {
                // TODO: #vasilenko UTF unuse std::string
                if (senderIpAddress == QString::fromStdString(conf->FromDHCP->Address)) {
                    return QString::fromStdString(ifacePtr->Info->HwAddress).toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString::fromStdString(ifacePtr->Info->HwAddress);
                }
            }

            std::vector<class onvifXsd__PrefixedIPv4Address*> addresses = conf->Manual;
            std::vector<class onvifXsd__PrefixedIPv4Address*>::const_iterator addrPtrIter = addresses.begin();

            for (; addrPtrIter != addresses.end(); ++addrPtrIter)
            {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;
                if (!addrPtr)
                    continue;

                // TODO: #vasilenko UTF unuse std::string
                if (senderIpAddress == QString::fromStdString(addrPtr->Address)) {
                    return QString::fromStdString(ifacePtr->Info->HwAddress).toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString::fromStdString(ifacePtr->Info->HwAddress);
                }
            }
        }
    }

    return someMacAddress.toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
}

void QnPlOnvifResource::setHostAddress(const QString &ip)
{
    //QnPhysicalCameraResource::se
    {
        QnMutexLocker lock( &m_mutex );

        QString mediaUrl = getMediaUrl();
        if (!mediaUrl.isEmpty())
        {
            QUrl url(mediaUrl);
            url.setHost(ip);
            setMediaUrl(url.toString());
        }

        QString onvifUrl = getDeviceOnvifUrl();
        if (!onvifUrl.isEmpty())
        {
            QUrl url(onvifUrl);
            url.setHost(ip);
            setDeviceOnvifUrl(url.toString());
        }
    }

    QnPhysicalCameraResource::setHostAddress(ip);
}

const QString QnPlOnvifResource::createOnvifEndpointUrl(const QString& ipAddress) {
    return QLatin1String(ONVIF_PROTOCOL_PREFIX) + ipAddress + QLatin1String(ONVIF_URL_SUFFIX);
}


typedef GSoapAsyncCallWrapper <
    DeviceSoapWrapper,
    NetIfacesReq,
    NetIfacesResp
> GSoapDeviceGetNetworkIntfAsyncWrapper;

void QnPlOnvifResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    QAuthenticator auth = getAuth();

    const QString deviceUrl = getDeviceOnvifUrl();
    if( deviceUrl.isEmpty() )
    {
        //calling completionHandler(false)
        nx::network::SocketGlobals::aioService().post(std::bind(completionHandler, false));
        return;
    }

    std::unique_ptr<DeviceSoapWrapper> soapWrapper( new DeviceSoapWrapper(
        deviceUrl.toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift ) );

    //Trying to get HardwareId
    auto asyncWrapper = std::make_shared<GSoapDeviceGetNetworkIntfAsyncWrapper>(
        std::move(soapWrapper),
        &DeviceSoapWrapper::getNetworkInterfaces );

    const QnMacAddress resourceMAC = getMAC();
    auto onvifCallCompletionFunc =
        [asyncWrapper, deviceUrl, resourceMAC, completionHandler]( int soapResultCode )
        {
            if( soapResultCode != SOAP_OK )
                return completionHandler( false );

            completionHandler(
                resourceMAC.toString() ==
                QnPlOnvifResource::fetchMacAddress( asyncWrapper->response(), QUrl(deviceUrl).host() ) );
        };

    NetIfacesReq request;
    asyncWrapper->callAsync(
        request,
        onvifCallCompletionFunc );
}

QString QnPlOnvifResource::getDriverName() const
{
    return MANUFACTURE;
}

const QSize QnPlOnvifResource::getVideoSourceSize() const
{
    QnMutexLocker lock( &m_mutex );
    return m_videoSourceSize;
}

int QnPlOnvifResource::getAudioBitrate() const
{
    return m_audioBitrate;
}

int QnPlOnvifResource::getAudioSamplerate() const
{
    return m_audioSamplerate;
}

QnPlOnvifResource::CODECS QnPlOnvifResource::getCodec(bool isPrimary) const
{
    QnMutexLocker lock( &m_mutex );
    return isPrimary ? m_primaryCodec : m_secondaryCodec;
}

void QnPlOnvifResource::setCodec(QnPlOnvifResource::CODECS c, bool isPrimary)
{
    QnMutexLocker lock( &m_mutex );
    if (isPrimary)
        m_primaryCodec = c;
    else
        m_secondaryCodec = c;
}

QnPlOnvifResource::AUDIO_CODECS QnPlOnvifResource::getAudioCodec() const
{
    QnMutexLocker lock( &m_mutex );
    return m_audioCodec;
}

void QnPlOnvifResource::setAudioCodec(QnPlOnvifResource::AUDIO_CODECS c)
{
    QnMutexLocker lock( &m_mutex );
    m_audioCodec = c;
}

QnAbstractStreamDataProvider* QnPlOnvifResource::createLiveDataProvider()
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool shouldAppearAsSingleChannel = resData.value<bool>(
        Qn::SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME);


    if (shouldAppearAsSingleChannel)
        return new nx::plugins::utils::MultisensorDataProvider(toSharedPointer(this));

    return new QnOnvifStreamReader(toSharedPointer());
}

void QnPlOnvifResource::setCroppingPhysical(QRect /*cropping*/)
{

}

CameraDiagnostics::Result QnPlOnvifResource::initInternal()
{
    auto result = QnPhysicalCameraResource::initInternal();
    if (!result)
        return result;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    CapabilitiesResp capabilitiesResponse;
    DeviceSoapWrapper* soapWrapper = nullptr;

    result = initOnvifCapabilitiesAndUrls(&capabilitiesResponse, &soapWrapper);
    if(!checkResultAndSetStatus(result))
        return result;

    std::unique_ptr<DeviceSoapWrapper> guard(soapWrapper);

    result = initializeMedia(capabilitiesResponse);
    if (!checkResultAndSetStatus(result))
        return result;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    initializeAdvancedParameters(capabilitiesResponse);

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    initializeIo(capabilitiesResponse);

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    initializePtz(capabilitiesResponse);

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    result = customInitialization(capabilitiesResponse);
    if (!checkResultAndSetStatus(result))
        return result;

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initOnvifCapabilitiesAndUrls(
    CapabilitiesResp* outCapabilitiesResponse,
    DeviceSoapWrapper** outDeviceSoapWrapper)
{
    *outDeviceSoapWrapper = nullptr;

    auto deviceOnvifUrl = getDeviceOnvifUrl();
    if (deviceOnvifUrl.isEmpty())
    {
        return m_prevOnvifResultCode.errorCode != CameraDiagnostics::ErrorCode::noError
            ? m_prevOnvifResultCode
            : CameraDiagnostics::RequestFailedResult(lit("getDeviceOnvifUrl"), QString());
    }

    calcTimeDrift();

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    QAuthenticator auth = getAuth();

    std::unique_ptr<DeviceSoapWrapper> deviceSoapWrapper =
        std::make_unique<DeviceSoapWrapper>(
            deviceOnvifUrl.toStdString(),
            auth.user(),
            auth.password(),
            m_timeDrift);

    auto result = fetchOnvifCapabilities(deviceSoapWrapper.get(), outCapabilitiesResponse);
    if (!result)
        return result;

    updateFirmware();
    fillFullUrlInfo(*outCapabilitiesResponse);

    if (getMediaUrl().isEmpty())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("ONVIF media URL is not filled by camera"));
    }

    *outDeviceSoapWrapper = deviceSoapWrapper.release();

    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::initializeMedia(
    const CapabilitiesResp& /*onvifCapabilities*/)
{
    setCodec(H264, true);
    setCodec(H264, false);

    auto result = fetchAndSetVideoSource();
    if (!result)
        return result;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    fetchAndSetAudioSource();

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    result = fetchAndSetResourceOptions();
    if (!result)
        return result;

    Qn::CameraCapabilities addFlags = Qn::NoCapabilities;
    int resolutionArea = m_primaryResolution.width() * m_primaryResolution.height();
    bool gotPrimaryStreamSoftMotionCapability = !m_primaryResolution.isEmpty()
        && resolutionArea <= MAX_PRIMARY_RES_FOR_SOFT_MOTION;

    if (gotPrimaryStreamSoftMotionCapability)
        addFlags |= Qn::PrimaryStreamSoftMotionCapability;

    if (addFlags != Qn::NoCapabilities)
        setCameraCapabilities(getCameraCapabilities() | addFlags);


    auto resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    if (getProperty(QnMediaResource::customAspectRatioKey()).isEmpty())
    {
        bool forcedAR = resourceData.value<bool>(QString("forceArFromPrimaryStream"), false);
        if (forcedAR && m_primaryResolution.height() > 0)
        {
            qreal ar = m_primaryResolution.width() / (qreal)m_primaryResolution.height();
            setCustomAspectRatio(ar);
        }

        QString defaultAR = resourceData.value<QString>(QString("defaultAR"));
        QStringList parts = defaultAR.split(L'x');
        if (parts.size() == 2)
        {
            qreal ar = parts[0].toFloat() / parts[1].toFloat();
            setCustomAspectRatio(ar);
        }
    }

	if (initializeTwoWayAudio())
        setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);

    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::initializePtz(const CapabilitiesResp& onvifCapabilities)
{
    bool result = fetchPtzInfo();
    if (!result)
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("Fetch Onvif PTZ configurations."),
            lit("Can not fetch Onvif PTZ configurations."));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initializeIo(const CapabilitiesResp& onvifCapabilities)
{
    const QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    m_isRelayOutputInversed = resourceData.value(QString("relayOutputInversed"), false);

    //registering onvif event handler
    std::vector<QnPlOnvifResource::RelayOutputInfo> relayOutputs;
    fetchRelayOutputs(&relayOutputs);
    if (!relayOutputs.empty())
    {
        setCameraCapability(Qn::RelayOutputCapability, true);
        //TODO #ak it's not clear yet how to get input port list for sure (on DW cam getDigitalInputs returns nothing)
        //but all cameras I've seen have either both input & output or none
        setCameraCapability(Qn::RelayInputCapability, true);

        //resetting all ports states to inactive
        for (std::vector<QnPlOnvifResource::RelayOutputInfo>::size_type
            i = 0;
            i < relayOutputs.size();
            ++i)
        {
            setRelayOutputStateNonSafe(0, QString::fromStdString(relayOutputs[i].token), false, 0);
        }
    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    fetchRelayInputInfo(onvifCapabilities);

    if (resourceData.contains(QString("relayInputCountForced")))
    {
        setCameraCapability(
            Qn::RelayInputCapability,
            resourceData.value<int>(QString("relayInputCountForced"), 0) > 0);
    }
    if (resourceData.contains(QString("relayOutputCountForced")))
    {
        setCameraCapability(
            Qn::RelayOutputCapability,
            resourceData.value<int>(QString("relayOutputCountForced"), 0) > 0);
    }
    if (resourceData.contains(QString("clearInputsTimeoutSec")))
    {
        m_clearInputsTimeoutUSec = resourceData.value<int>(
            QString("clearInputsTimeoutSec"), 0) * 1000 * 1000;
    }

    QnIOPortDataList allPorts = getRelayOutputList();

    if (onvifCapabilities.Capabilities &&
        onvifCapabilities.Capabilities->Device &&
        onvifCapabilities.Capabilities->Device->IO &&
        onvifCapabilities.Capabilities->Device->IO->InputConnectors &&
        *onvifCapabilities.Capabilities->Device->IO->InputConnectors > 0)
    {

        const auto portsCount = *onvifCapabilities.Capabilities
            ->Device
            ->IO
            ->InputConnectors;

        m_inputPortCount = portsCount;

        if (portsCount <= (int)MAX_IO_PORTS_PER_DEVICE)
        {
            for (int i = 1; i <= portsCount; ++i)
            {
                QnIOPortData inputPort;
                inputPort.portType = Qn::PT_Input;
                inputPort.id = lit("%1").arg(i);
                inputPort.inputName = tr("Input %1").arg(i);
                allPorts.emplace_back(std::move(inputPort));
            }
        }
        else
        {
            NX_LOGX(lit("Device has too many input ports. Url: %1")
                .arg(getUrl()), cl_logDEBUG1);
        }
    }

    setIOPorts(std::move(allPorts));

    m_portNamePrefixToIgnore = resourceData.value<QString>(
        lit("portNamePrefixToIgnore"), QString());

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initializeAdvancedParameters(
    const CapabilitiesResp& /*onvifCapabilities*/)
{
    fetchAndSetAdvancedParameters();
    return CameraDiagnostics::NoErrorResult();
}

QSize QnPlOnvifResource::getMaxResolution() const
{
    QnMutexLocker lock( &m_mutex );
    return m_resolutionList.isEmpty()? EMPTY_RESOLUTION_PAIR: m_resolutionList.front();
}

QSize QnPlOnvifResource::getNearestResolutionForSecondary(const QSize& resolution, float aspectRatio) const
{
    QnMutexLocker lock( &m_mutex );
    return getNearestResolution(resolution, aspectRatio, SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height(), m_secondaryResolutionList);
}

int QnPlOnvifResource::suggestBitrateKbps(const QSize& resolution, const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    return strictBitrate(QnPhysicalCameraResource::suggestBitrateKbps(resolution, streamParams, role), role);
}

int QnPlOnvifResource::strictBitrate(int bitrate, Qn::ConnectionRole role) const
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));

    QString availableBitratesParamName;
    QString bitrateBoundsParamName;

    quint64 bestBitrate = bitrate;

    if (role == Qn::CR_LiveVideo)
    {
        bitrateBoundsParamName = Qn::HIGH_STREAM_BITRATE_BOUNDS_PARAM_NAME;
        availableBitratesParamName = Qn::HIGH_STREAM_AVAILABLE_BITRATES_PARAM_NAME;
    }
    else if (role == Qn::CR_SecondaryLiveVideo)
    {
        bitrateBoundsParamName = Qn::LOW_STREAM_AVAILABLE_BITRATES_PARAM_NAME;
        availableBitratesParamName = Qn::LOW_STREAM_AVAILABLE_BITRATES_PARAM_NAME;
    }

    if (!bitrateBoundsParamName.isEmpty())
    {
        auto bounds = resData.value<QnBounds>(bitrateBoundsParamName, QnBounds());
        if (!bounds.isNull())
            bestBitrate = qMin(bounds.max, qMax(bounds.min, bestBitrate));
    }

    if (availableBitratesParamName.isEmpty())
        return bestBitrate;

    auto availableBitrates = resData.value<QnBitrateList>(availableBitratesParamName, QnBitrateList());

    quint64 bestDiff = std::numeric_limits<quint64>::max();
    for (const auto& bitrateOption: availableBitrates)
    {
        auto diff = qMax<quint64>(bitrateOption, bitrate) - qMin<quint64>(bitrateOption, bitrate);

        if (diff < bestDiff)
        {
            bestDiff = diff;
            bestBitrate = bitrateOption;
        }
    }

    return bestBitrate;
}

void QnPlOnvifResource::checkPrimaryResolution(QSize& primaryResolution)
{
    for (uint i = 0; i < sizeof(strictResolutionList) / sizeof(StrictResolution); ++i)
    {
        if (getModel() == QLatin1String(strictResolutionList[i].model))
            primaryResolution = strictResolutionList[i].maxRes;
    }
}

QSize QnPlOnvifResource::findSecondaryResolution(const QSize& primaryRes, const QList<QSize>& secondaryResList, double* matchCoeff)
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));

    auto forcedSecondaryResolution = resData.value<QString>(
        Qn::FORCED_SECONDARY_STREAM_RESOLUTION_PARAM_NAME);

    if (!forcedSecondaryResolution.isEmpty())
    {
        auto split = forcedSecondaryResolution.split('x');
        if (split.size() == 2)
        {
            QSize res;
            res.setWidth(split[0].toInt());
            res.setHeight(split[1].toInt());
            return res;
        }
        else
        {
            qWarning()
                << "QnPlOnvifResource::findSecondaryResolution(). "
                << "Wrong parameter format (FORCED_SECONDARY_STREAM_RESOLUTION_PARAM_NAME) "
                << forcedSecondaryResolution;
        }
    }

    float currentAspect = getResolutionAspectRatio(primaryRes);
    int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();
    QSize result = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect, maxSquare, secondaryResList, matchCoeff);
    if (result == EMPTY_RESOLUTION_PAIR)
        result = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0, maxSquare, secondaryResList, matchCoeff); // try to get resolution ignoring aspect ration
    return result;
}

void QnPlOnvifResource::fetchAndSetPrimarySecondaryResolution()
{
    QnMutexLocker lock( &m_mutex );

    if (m_resolutionList.isEmpty()) {
        return;
    }

    m_primaryResolution = m_resolutionList.front();
    checkPrimaryResolution(m_primaryResolution);

    m_secondaryResolution = findSecondaryResolution(m_primaryResolution, m_secondaryResolutionList);
    float currentAspect = getResolutionAspectRatio(m_primaryResolution);


    if (m_secondaryResolution != EMPTY_RESOLUTION_PAIR) {
        NX_ASSERT(m_secondaryResolution.width() <= SECONDARY_STREAM_MAX_RESOLUTION.width() &&
            m_secondaryResolution.height() <= SECONDARY_STREAM_MAX_RESOLUTION.height());
        return;
    }

    double maxSquare = m_primaryResolution.width() * m_primaryResolution.height();

    for (const QSize& resolution: m_resolutionList) {
        float aspect = getResolutionAspectRatio(resolution);

        if (std::abs(aspect - currentAspect) < MAX_EPS) {
            continue;
        }
        currentAspect = aspect;

        double square = resolution.width() * resolution.height();
        if (square <= 0.90 * maxSquare) {
            break;
        }

        QSize tmp = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect);
        if (tmp != EMPTY_RESOLUTION_PAIR) {
            m_primaryResolution = resolution;
            m_secondaryResolution = tmp;

            NX_ASSERT(m_secondaryResolution.width() <= SECONDARY_STREAM_MAX_RESOLUTION.width() &&
                m_secondaryResolution.height() <= SECONDARY_STREAM_MAX_RESOLUTION.height());

            return;
        }
    }
}

QSize QnPlOnvifResource::getPrimaryResolution() const
{
    QnMutexLocker lock( &m_mutex );
    return m_primaryResolution;
}

void QnPlOnvifResource::setPrimaryResolution(const QSize& value)
{
    m_primaryResolution = value;
}

QSize QnPlOnvifResource::getSecondaryResolution() const
{
    QnMutexLocker lock( &m_mutex );
    return m_secondaryResolution;
}

int QnPlOnvifResource::getPrimaryH264Profile() const
{
    QnMutexLocker lock( &m_mutex );
    return m_primaryH264Profile;
}

int QnPlOnvifResource::getSecondaryH264Profile() const
{
    QnMutexLocker lock( &m_mutex );
    return m_secondaryH264Profile;
}

void QnPlOnvifResource::setMaxFps(int f)
{
    setProperty(Qn::MAX_FPS_PARAM_NAME, f);
}

const QString QnPlOnvifResource::getPrimaryVideoEncoderId() const
{
    QnMutexLocker lock( &m_mutex );
    return m_primaryVideoEncoderId;
}

const QString QnPlOnvifResource::getSecondaryVideoEncoderId() const
{
    QnMutexLocker lock( &m_mutex );
    return m_secondaryVideoEncoderId;
}

const QString QnPlOnvifResource::getAudioEncoderId() const
{
    QnMutexLocker lock( &m_mutex );
    return m_audioEncoderId;
}

const QString QnPlOnvifResource::getVideoSourceId() const
{
    QnMutexLocker lock( &m_mutex );
    return m_videoSourceId;
}

const QString QnPlOnvifResource::getAudioSourceId() const
{
    QnMutexLocker lock( &m_mutex );
    return m_audioSourceId;
}

QString QnPlOnvifResource::getDeviceOnvifUrl() const
{
	QnMutexLocker lock(&m_mutex);
	if (m_serviceUrls.deviceServiceUrl.isEmpty())
		m_serviceUrls.deviceServiceUrl = getProperty(ONVIF_URL_PARAM_NAME);

	return m_serviceUrls.deviceServiceUrl;
}

void QnPlOnvifResource::setDeviceOnvifUrl(const QString& src)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_serviceUrls.deviceServiceUrl = src;
    }

    setProperty(ONVIF_URL_PARAM_NAME, src);
}
QString QnPlOnvifResource::fromOnvifDiscoveredUrl(const std::string& onvifUrl, bool updatePort)
{
    QUrl url(QString::fromStdString(onvifUrl));
    QUrl mediaUrl(getUrl());
    url.setHost(getHostAddress());
    if (updatePort && mediaUrl.port(-1) != -1)
        url.setPort(mediaUrl.port());
    return url.toString();
}

CameraDiagnostics::Result QnPlOnvifResource::readDeviceInformation()
{
    OnvifResExtInfo extInfo;

    QAuthenticator auth = getAuth();
    CameraDiagnostics::Result result =  readDeviceInformation(getDeviceOnvifUrl(), auth, m_timeDrift, &extInfo);
    if (result) {
        if (getName().isEmpty())
            setName(extInfo.name);
        if (getModel().isEmpty())
            setModel(extInfo.model);
        if (getFirmware().isEmpty())
            setFirmware(extInfo.firmware);
        if (getMAC().isNull())
            setMAC(QnMacAddress(extInfo.mac));
        if (getVendor() == lit("ONVIF") && !extInfo.vendor.isNull())
            setVendor(extInfo.vendor); // update default vendor
        if (getPhysicalId().isEmpty()) {
            QString id = extInfo.hardwareId;
            if (!extInfo.serial.isEmpty())
                id += QString(L'_') + extInfo.serial;
            setPhysicalId(id);
        }
    }
    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::readDeviceInformation(const QString& onvifUrl, const QAuthenticator& auth, int timeDrift, OnvifResExtInfo* extInfo)
{
    if (timeDrift == INT_MAX)
        timeDrift = calcTimeDrift(onvifUrl);

    DeviceSoapWrapper soapWrapper(onvifUrl.toStdString(), auth.user(), auth.password(), timeDrift);

    DeviceInfoReq request;
    DeviceInfoResp response;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
            << soapWrapper.getEndpointUrl() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        if (soapWrapper.isNotAuthenticated())
            return CameraDiagnostics::NotAuthorisedResult( onvifUrl );

        return CameraDiagnostics::RequestFailedResult(QLatin1String("getDeviceInformation"), soapWrapper.getLastError());
    }
    else
    {
        extInfo->name = QString::fromStdString(response.Manufacturer) + QLatin1String(" - ") + QString::fromStdString(response.Model);
        extInfo->model = QString::fromStdString(response.Model);
        extInfo->firmware = QString::fromStdString(response.FirmwareVersion);
        extInfo->vendor = QString::fromStdString(response.Manufacturer);
        extInfo->hardwareId = QString::fromStdString(response.HardwareId);
        extInfo->serial = QString::fromStdString(response.SerialNumber);
    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    //Trying to get MAC
    NetIfacesReq requestIfList;
    NetIfacesResp responseIfList;

    soapRes = soapWrapper.getNetworkInterfaces(requestIfList, responseIfList); // this request is optional
    if( soapRes != SOAP_OK )
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch MAC address. Reason: SOAP to endpoint "
            << onvifUrl << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
#endif
    }
    else {
        extInfo->mac = fetchMacAddress(responseIfList, QUrl(onvifUrl).host());
    }

    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::notificationReceived(
    const oasisWsnB2__NotificationMessageHolderType& notification,
    time_t minNotificationTime )
{
    const auto now = qnSyncTime->currentUSecsSinceEpoch();
    if( m_clearInputsTimeoutUSec > 0 )
    {
        for( auto& state: m_relayInputStates )
        {
            if( state.second.value && state.second.timestamp + m_clearInputsTimeoutUSec < now)
            {
                state.second.value = false;
                state.second.timestamp = now;
                onRelayInputStateChange( state.first, state.second );
            }
        }
    }

    if( !notification.Message.__any )
    {
        NX_LOGX( lit("Received notification with empty message. Ignoring..."), cl_logDEBUG2 );
        return;
    }

    if( !notification.oasisWsnB2__Topic ||
        !notification.oasisWsnB2__Topic->__item )
    {
        NX_LOGX( lit("Received notification with no topic specified. Ignoring..."), cl_logDEBUG2 );
        return;
    }

    QString eventTopic( QLatin1String(notification.oasisWsnB2__Topic->__item) );

    NX_LOGX(lit("%1 Recevied notification %2").arg(getUrl()).arg(eventTopic), cl_logDEBUG2);

    //eventTopic may have namespaces. E.g., ns:Device/ns:Trigger/ns:Relay,
        //but we want Device/Trigger/Relay. Fixing...
    QStringList eventTopicTokens = eventTopic.split( QLatin1Char('/') );
    for( QString& token: eventTopicTokens )
    {
        int nsSepIndex = token.indexOf( QLatin1Char(':') );
        if( nsSepIndex != -1 )
            token.remove( 0, nsSepIndex+1 );
    }
    eventTopic = eventTopicTokens.join( QLatin1Char('/') );

    if( eventTopic.indexOf( lit("Trigger/Relay") ) == -1 &&
        eventTopic.indexOf( lit("IO/Port") ) == -1 &&
        eventTopic.indexOf( lit("Trigger/DigitalInput") ) == -1 &&
        eventTopic.indexOf( lit("Device/IO/VirtualPort") ) == -1)
    {
        NX_LOGX( lit("Received notification with unknown topic: %1. Ignoring...").
            arg(QLatin1String(notification.oasisWsnB2__Topic->__item)), cl_logDEBUG2 );
        return;
    }

    //parsing Message
    QXmlSimpleReader reader;
    NotificationMessageParseHandler handler( m_cameraTimeZone );
    reader.setContentHandler( &handler );
    QBuffer srcDataBuffer;
    srcDataBuffer.setData(
        notification.Message.__any,
        (int) strlen(notification.Message.__any) );
    QXmlInputSource xmlSrc( &srcDataBuffer );
    if( !reader.parse( &xmlSrc ) )
        return;

    if( (minNotificationTime != (time_t)-1) && (handler.utcTime.toTime_t() < minNotificationTime) )
        return; //ignoring old notifications: DW camera can deliver old cached notifications

    bool sourceIsExplicitRelayInput = false;

    //checking that there is single source and this source is a relay port
    std::list<NotificationMessageParseHandler::SimpleItem>::const_iterator portSourceIter = handler.source.end();
    for( std::list<NotificationMessageParseHandler::SimpleItem>::const_iterator
        it = handler.source.begin();
        it != handler.source.end();
        ++it )
    {
        if( it->name == lit("port") ||
            it->name == lit("RelayToken") ||
            it->name == lit("Index") )
        {
            portSourceIter = it;
            break;
        }
        else if (it->name == lit("InputToken") ||
                 it->name == lit("RelayInputToken"))
        {
            sourceIsExplicitRelayInput = true;
            portSourceIter = it;
            break;
        }
    }

    if( portSourceIter == handler.source.end()  //source is not port
        || (handler.data.name != lit("LogicalState") &&
            handler.data.name != lit("state") &&
            handler.data.name != lit("Level") &&
            handler.data.name != lit("RelayLogicalState"))
        )
    {
        return;
    }

    //some cameras (especially, Vista) send here events on output port, filtering them out
    const bool sourceNameMatchesRelayOutPortName =
        std::find_if(
            m_relayOutputInfo.begin(),
            m_relayOutputInfo.end(),
            [&handler](const RelayOutputInfo& outputInfo) {
                return QString::fromStdString(outputInfo.token) == handler.source.front().value;
            }) != m_relayOutputInfo.end();
    const bool sourceIsRelayOutPort =
        (!m_portNamePrefixToIgnore.isEmpty() && handler.source.front().value.startsWith(m_portNamePrefixToIgnore)) ||
        sourceNameMatchesRelayOutPortName;
    if (!sourceIsExplicitRelayInput &&
        !handler.source.empty() &&
        sourceIsRelayOutPort)
    {
        return; //this is notification about output port
    }

    //saving port state
    const bool newValue = (handler.data.value == lit("true")) || (handler.data.value == lit("active")) || (handler.data.value.toInt() > 0);
    auto& state = m_relayInputStates[portSourceIter->value];
    state.timestamp = now;
    if( state.value != newValue )
    {
        state.value = newValue;
        onRelayInputStateChange( portSourceIter->value, state );
    }
}

void QnPlOnvifResource::onRelayInputStateChange(const QString& name, const RelayInputState& state)
{
    QString portId = name;
    {
        bool success = false;
        int intPortId = portId.toInt(&success);
        // Onvif device enumerates ports from 1. see 'allPorts' filling code.
        if (success)
            portId = QString::number(intPortId + 1);
    }

    size_t aliasesCount = m_portAliases.size();
    if (aliasesCount && aliasesCount == m_inputPortCount)
    {
        for (size_t i = 0; i < aliasesCount; i++)
        {
            if (m_portAliases[i] == name)
            {
                portId = lit("%1").arg(i + 1);
                break;
            }
        }
    }
    else
    {
        auto portIndex = getInputPortNumberFromString(name);

        if (!portIndex.isEmpty())
            portId = portIndex;
    }

    NX_DEBUG(this, lm("Input port '%1' = %2").args(portId, state.value));
    emit cameraInput(
        toSharedPointer(),
        portId,
        state.value,
        state.timestamp);
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetResourceOptions()
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);

    CameraDiagnostics::Result result = fetchAndSetVideoEncoderOptions(soapWrapper);
    if (!result)
        return result;

    result = updateResourceCapabilities();
    if (!result)
        return result;

    //All VideoEncoder options are set, so we can calculate resolutions for the streams
    fetchAndSetPrimarySecondaryResolution();

    NX_LOGX(QString(lit("ONVIF debug: got primary resolution %1x%2 for camera %3")).
        arg(m_primaryResolution.width()).arg(m_primaryResolution.height()).arg(getHostAddress()), cl_logDEBUG1);
    NX_LOGX(QString(lit("ONVIF debug: got secondary resolution %1x%2 for camera %3")).
        arg(m_secondaryResolution.width()).arg(m_secondaryResolution.height()).arg(getHostAddress()), cl_logDEBUG1);


    //Before invoking <fetchAndSetHasDualStreaming> Primary and Secondary Resolutions MUST be set
    fetchAndSetDualStreaming(soapWrapper);

    if (fetchAndSetAudioEncoder(soapWrapper) && fetchAndSetAudioEncoderOptions(soapWrapper))
        setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, 1);
    else
        setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, QString(lit("0")));

    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::updateSecondaryResolutionList(const VideoOptionsLocal& opts)
{
    QnMutexLocker lock( &m_mutex );
    m_secondaryResolutionList = opts.resolutions;
    std::sort(m_secondaryResolutionList.begin(), m_secondaryResolutionList.end(), resolutionGreaterThan);
}

void QnPlOnvifResource::setVideoEncoderOptions(const VideoOptionsLocal& opts) {
    if (opts.minQ != -1)
    {
        setMinMaxQuality(opts.minQ, opts.maxQ);

        NX_LOGX(QString(lit("ONVIF quality range [%1, %2]")).arg(m_minQuality).arg(m_maxQuality), cl_logDEBUG1);

    }
#ifdef PL_ONVIF_DEBUG
    else
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: camera didn't return quality range. UniqueId: " << getUniqueId();
    }
#endif

    if (opts.isH264)
        setVideoEncoderOptionsH264(opts);
    else
        setVideoEncoderOptionsJpeg(opts);
}

void QnPlOnvifResource::setVideoEncoderOptionsH264(const VideoOptionsLocal& opts)
{

    if (opts.frameRateMax > 0)
        setMaxFps(opts.frameRateMax);
#ifdef PL_ONVIF_DEBUG
    else
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Max FPS. UniqueId: " << getUniqueId();
#endif

    if (opts.govMin >= 0)
    {
        QnMutexLocker lock( &m_mutex );
        m_iframeDistance = DEFAULT_IFRAME_DISTANCE <= opts.govMin ? opts.govMin : (DEFAULT_IFRAME_DISTANCE >= opts.govMax ? opts.govMax: DEFAULT_IFRAME_DISTANCE);
        NX_LOGX(QString(lit("ONVIF iframe distance: %1")).arg(m_iframeDistance), cl_logDEBUG1);
    }
#ifdef PL_ONVIF_DEBUG
    else
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Iframe Distance. UniqueId: " << getUniqueId();
    }
#endif

    if (opts.resolutions.isEmpty())
    {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Resolutions. UniqueId: " << getUniqueId();
#endif
    }
    else
    {
        QnMutexLocker lock( &m_mutex );
        m_resolutionList = opts.resolutions;
        std::sort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);

    }

    QnMutexLocker lock( &m_mutex );

    //Printing fetched resolutions
    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug))
    {
        NX_LOGX(QString(lit("ONVIF resolutions:")), cl_logDEBUG1);
        for (const QSize& resolution: m_resolutionList)
        {
            NX_LOGX(QString(lit("%1x%2"))
                .arg(resolution.width())
                .arg(resolution.height()), cl_logDEBUG1);
        }
    }
}

void QnPlOnvifResource::setVideoEncoderOptionsJpeg(const VideoOptionsLocal& opts)
{
    if (opts.frameRateMax > 0)
        setMaxFps(opts.frameRateMax);
#ifdef PL_ONVIF_DEBUG
    else
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Max FPS. UniqueId: " << getUniqueId();
#endif

    if (opts.resolutions.isEmpty())
    {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Resolutions. UniqueId: " << getUniqueId();
#endif
    }
    else
    {
        QnMutexLocker lock( &m_mutex );
        m_resolutionList = opts.resolutions;
        std::sort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);
    }

    QnMutexLocker lock( &m_mutex );
    //Printing fetched resolutions
    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug))
    {
        NX_LOGX(QString(lit("ONVIF resolutions:")), cl_logDEBUG1);
        for (const QSize& resolution: m_resolutionList) {
            NX_LOGX(QString(lit("%1x%2")).arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
        }
    }
}

int QnPlOnvifResource::innerQualityToOnvif(Qn::StreamQuality quality) const
{
    if (quality > Qn::QualityHighest) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::innerQualityToOnvif: got unexpected quality (too big): " << quality;
#endif
        return m_maxQuality;
    }
    if (quality < Qn::QualityLowest) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::innerQualityToOnvif: got unexpected quality (too small): " << quality;
#endif
        return m_minQuality;
    }

    NX_LOGX(QString(lit("QnPlOnvifResource::innerQualityToOnvif: in quality = %1, out qualty = %2, minOnvifQuality = %3, maxOnvifQuality = %4"))
            .arg(quality)
            .arg(m_minQuality + (m_maxQuality - m_minQuality) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest))
            .arg(m_minQuality)
            .arg(m_maxQuality), cl_logDEBUG1);

    return m_minQuality + (m_maxQuality - m_minQuality) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);
}

/*
bool QnPlOnvifResource::isSoapAuthorized() const
{
    QAuthenticator auth(getAuth());
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    qDebug() << "QnPlOnvifResource::isSoapAuthorized: m_deviceOnvifUrl is '" << getDeviceOnvifUrl() << "'";
    qDebug() << "QnPlOnvifResource::isSoapAuthorized: login = " << soapWrapper.getLogin() << ", password = " << soapWrapper.getPassword();

    NetIfacesReq request;
    NetIfacesResp response;
    int soapRes = soapWrapper.getNetworkInterfaces(request, response);

    if (soapRes != SOAP_OK && soapWrapper.isNotAuthenticated())
    {
        return false;
    }

    return true;
}
*/

int QnPlOnvifResource::getTimeDrift() const
{
    if (m_timeDriftTimer.elapsed() > MAX_TIME_DRIFT_UPDATE_PERIOD_MS)
        calcTimeDrift();

    return m_timeDrift;
}

void QnPlOnvifResource::setTimeDrift(int value)
{
    m_timeDrift = value;
    m_timeDriftTimer.restart();
}

void QnPlOnvifResource::calcTimeDrift(int* outSoapRes) const
{
    m_timeDrift = calcTimeDrift(getDeviceOnvifUrl(), outSoapRes, &m_cameraTimeZone);
    m_timeDriftTimer.restart();
}

int QnPlOnvifResource::calcTimeDrift(const QString& deviceUrl, int* outSoapRes, QTimeZone* timeZone)
{
    DeviceSoapWrapper soapWrapper(deviceUrl.toStdString(), QString(), QString(), 0);

    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = soapWrapper.GetSystemDateAndTime(request, response);
    if (outSoapRes)
        *outSoapRes = soapRes;

    if (soapRes == SOAP_OK && response.SystemDateAndTime && response.SystemDateAndTime->UTCDateTime)
    {
        if (timeZone && response.SystemDateAndTime->TimeZone)
            *timeZone = QTimeZone(response.SystemDateAndTime->TimeZone->TZ.c_str());

        onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
        onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;
        if (!date || !time)
            return 0;

        QDateTime datetime(QDate(date->Year, date->Month, date->Day), QTime(time->Hour, time->Minute, time->Second), Qt::UTC);
        int drift = datetime.toMSecsSinceEpoch()/MS_PER_SECOND - QDateTime::currentMSecsSinceEpoch()/MS_PER_SECOND;
        return drift;
    }
    return 0;
}

QString QnPlOnvifResource::getMediaUrl() const
{
	QnMutexLocker lock(&m_mutex);
	return m_serviceUrls.mediaServiceUrl;

    //return getProperty(MEDIA_URL_PARAM_NAME);
}

void QnPlOnvifResource::setMediaUrl(const QString& src)
{
	QnMutexLocker lock(&m_mutex);
	m_serviceUrls.mediaServiceUrl = src;

    //setProperty(MEDIA_URL_PARAM_NAME, src);
}

QString QnPlOnvifResource::getImagingUrl() const
{
    QnMutexLocker lock( &m_mutex );
    return m_serviceUrls.imagingServiceUrl;
}

void QnPlOnvifResource::setImagingUrl(const QString& src)
{
    QnMutexLocker lock( &m_mutex );
    m_serviceUrls.imagingServiceUrl = src;
}

QString QnPlOnvifResource::getVideoSourceToken() const {
    QnMutexLocker lock( &m_mutex );
    return m_videoSourceToken;
}

void QnPlOnvifResource::setVideoSourceToken(const QString &src) {
    QnMutexLocker lock( &m_mutex );
    m_videoSourceToken = src;
}

QString QnPlOnvifResource::getPtzUrl() const
{
    QnMutexLocker lock( &m_mutex );
    return m_serviceUrls.ptzServiceUrl;
}

void QnPlOnvifResource::setPtzUrl(const QString& src)
{
    QnMutexLocker lock( &m_mutex );
    m_serviceUrls.ptzServiceUrl = src;
}

QString QnPlOnvifResource::getPtzConfigurationToken() const {
    QnMutexLocker lock( &m_mutex );
    return m_ptzConfigurationToken;
}

void QnPlOnvifResource::setPtzConfigurationToken(const QString &src) {
    QnMutexLocker lock( &m_mutex );
    m_ptzConfigurationToken = src;
}

QString QnPlOnvifResource::getPtzProfileToken() const
{
    QnMutexLocker lock( &m_mutex );
    return m_ptzProfileToken;
}

void QnPlOnvifResource::setPtzProfileToken(const QString& src)
{
    QnMutexLocker lock( &m_mutex );
    m_ptzProfileToken = src;
}

void QnPlOnvifResource::setMinMaxQuality(int min, int max)
{
    int netoptixDelta = Qn::QualityHighest - Qn::QualityLowest;
    int onvifDelta = max - min;

    if (netoptixDelta < 0 || onvifDelta < 0)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::setMinMaxQuality: incorrect values: min > max: onvif ["
                   << min << ", " << max << "] netoptix [" << Qn::QualityLowest << ", " << Qn::QualityHighest << "]";
#endif
        return;
    }

    float coef = (1 - ((float)netoptixDelta) / onvifDelta) * 0.5;
    coef = coef <= 0? 0.0: (coef <= QUALITY_COEF? coef: QUALITY_COEF);
    int shift = round(onvifDelta * coef);

    m_minQuality = min + shift;
    m_maxQuality = max - shift;
}

int QnPlOnvifResource::round(float value)
{
    float floorVal = floorf(value);
    return floorVal - value < 0.5? (int)value: (int)value + 1;
}


bool QnPlOnvifResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source)
{
    QnPlOnvifResourcePtr onvifR = source.dynamicCast<QnPlOnvifResource>();
    if (!onvifR)
        return false;

    bool result = QnPhysicalCameraResource::mergeResourcesIfNeeded(source);

    QString onvifUrlSource = onvifR->getDeviceOnvifUrl();
    if (!onvifUrlSource.isEmpty() && getDeviceOnvifUrl() != onvifUrlSource)
    {
        setDeviceOnvifUrl(onvifUrlSource);
        result = true;
    }

    return result;
}

static QString getRelayOutpuToken( const QnPlOnvifResource::RelayOutputInfo& relayInfo )
{
    return QString::fromStdString( relayInfo.token );
}

//!Implementation of QnSecurityCamResource::getRelayOutputList
QnIOPortDataList QnPlOnvifResource::getRelayOutputList() const
{
    QStringList idList;
    std::transform(
        m_relayOutputInfo.begin(),
        m_relayOutputInfo.end(),
        std::back_inserter(idList),
        getRelayOutpuToken );
    QnIOPortDataList result;
    for (const auto& data: idList) {
        QnIOPortData value;
        value.portType = Qn::PT_Output;
        value.id = data;
        value.outputName = tr("Output %1").arg(data);
        result.push_back(value);
    }
    return result;
}

QnIOPortDataList QnPlOnvifResource::getInputPortList() const
{
    //TODO/IMPL
    return QnIOPortDataList();
}

bool QnPlOnvifResource::fetchRelayInputInfo( const CapabilitiesResp& capabilitiesResponse )
{
    if( m_deviceIOUrl.empty() )
        return false;

    if( capabilitiesResponse.Capabilities &&
        capabilitiesResponse.Capabilities->Device &&
        capabilitiesResponse.Capabilities->Device->IO &&
        capabilitiesResponse.Capabilities->Device->IO->InputConnectors &&
        *capabilitiesResponse.Capabilities->Device->IO->InputConnectors > 0  &&
        *capabilitiesResponse.Capabilities->Device->IO->InputConnectors < (int) MAX_IO_PORTS_PER_DEVICE)
    {
        //camera has input port
        setCameraCapability( Qn::RelayInputCapability, true );
    }

    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    m_portAliases = resData.value<std::vector<QString>>(Qn::ONVIF_INPUT_PORT_ALIASES_PARAM_NAME);

    if (!m_portAliases.empty())
        return true;

    QAuthenticator auth = getAuth();
    DeviceIOWrapper soapWrapper(
        m_deviceIOUrl,
        auth.user(),
        auth.password(),
        m_timeDrift );

    _onvifDeviceIO__GetDigitalInputs request;
    _onvifDeviceIO__GetDigitalInputsResponse response;
    const int soapCallResult = soapWrapper.getDigitalInputs( request, response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOGX( lit("Failed to get relay digital input list. endpoint %1")
            .arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
        return true;
    }

    m_portAliases.clear();
    for ( const auto& input: response.DigitalInputs)
        m_portAliases.push_back(QString::fromStdString(input->token));

    return true;
}

bool QnPlOnvifResource::fetchPtzInfo() {

    if(getPtzUrl().isEmpty())
        return false;

    QAuthenticator auth = getAuth();
    PtzSoapWrapper ptz (getPtzUrl().toStdString(), auth.user(), auth.password(), getTimeDrift());

    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) == SOAP_OK && response.PTZConfiguration.size() > 0)
        m_ptzConfigurationToken = QString::fromStdString(response.PTZConfiguration[0]->token);

    return true;
}

//!Implementation of QnSecurityCamResource::setRelayOutputState
bool QnPlOnvifResource::setRelayOutputState(
    const QString& outputID,
    bool active,
    unsigned int autoResetTimeoutMS )
{
    QnMutexLocker lk( &m_ioPortMutex );

    using namespace std::placeholders;
    const quint64 timerID = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&QnPlOnvifResource::setRelayOutputStateNonSafe, this, _1, outputID, active, autoResetTimeoutMS),
        std::chrono::milliseconds::zero());
    m_triggerOutputTasks[timerID] = TriggerOutputTask(outputID, active, autoResetTimeoutMS);
    return true;
}

int QnPlOnvifResource::getH264StreamProfile(const VideoOptionsLocal& videoOptionsLocal)
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));

    auto desiredH264Profile = resData.value<QString>(Qn::DESIRED_H264_PROFILE_PARAM_NAME);

    if (videoOptionsLocal.h264Profiles.isEmpty())
        return -1;
    else if (!desiredH264Profile.isEmpty())
        return fromStringToH264Profile(desiredH264Profile);
    else
        return (int) videoOptionsLocal.h264Profiles[0];
}

bool QnPlOnvifResource::isH264Allowed() const
{
    return true;
    //bool blockH264 = getName().contains(QLatin1String("SEYEON TECH")) && getModel().contains(QLatin1String("FW3471"));
    //return !blockH264;
}

qreal QnPlOnvifResource::getBestSecondaryCoeff(const QList<QSize> resList, qreal aspectRatio) const
{
    int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();
    QSize secondaryRes = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, aspectRatio, maxSquare, resList);
    if (secondaryRes == EMPTY_RESOLUTION_PAIR)
        secondaryRes = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0, maxSquare, resList);

    qreal secResSquare = SECONDARY_STREAM_DEFAULT_RESOLUTION.width() * SECONDARY_STREAM_DEFAULT_RESOLUTION.height();
    qreal findResSquare = secondaryRes.width() * secondaryRes.height();
    if (findResSquare > secResSquare)
        return findResSquare / secResSquare;
    else
        return secResSquare / findResSquare;
}

int QnPlOnvifResource::getSecondaryIndex(const QList<VideoOptionsLocal>& optList) const
{
    if (optList.size() < 2 || optList[0].resolutions.isEmpty())
        return 1; // default value

    qreal bestResCoeff = INT_MAX;
    int bestResIndex = 1;
    bool bestIsH264 = false;

    qreal aspectRation = (qreal) optList[0].resolutions[0].width() / (qreal) optList[0].resolutions[0].height();

    for (int i = 1; i < optList.size(); ++i)
    {
        qreal resCoeff = getBestSecondaryCoeff(optList[i].resolutions, aspectRation);
        if (resCoeff < bestResCoeff || (resCoeff == bestResCoeff && optList[i].isH264 && !bestIsH264)) {
            bestResCoeff = resCoeff;
            bestResIndex = i;
            bestIsH264 = optList[i].isH264;
        }
    }

    return bestResIndex;
}

bool QnPlOnvifResource::registerNotificationConsumer()
{
    if (m_appStopping)
        return false;

    QnMutexLocker lk( &m_ioPortMutex );

    //determining local address, accessible by onvif device
    QUrl eventServiceURL( QString::fromStdString(m_eventCapabilities->XAddr) );
    QString localAddress;

    // TODO: #ak should read local address only once
    std::unique_ptr<AbstractStreamSocket> sock( SocketFactory::createStreamSocket() );
    if( !sock->connect( eventServiceURL.host(), eventServiceURL.port(nx_http::DEFAULT_HTTP_PORT) ) )
    {
        NX_LOGX( lit("Failed to connect to %1:%2 to determine local address. %3").
            arg(eventServiceURL.host()).arg(eventServiceURL.port()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
        return false;
    }
    localAddress = sock->getLocalAddress().address.toString();

    QAuthenticator auth = getAuth();

    NotificationProducerSoapWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user(),
        auth.password(),
        m_timeDrift );
    soapWrapper.getProxy()->soap->imode |= SOAP_XML_IGNORENS;

    char buf[512];

    //providing local gsoap server url
    _oasisWsnB2__Subscribe request;
    ns1__EndpointReferenceType notificationConsumerEndPoint;
    ns1__AttributedURIType notificationConsumerEndPointAddress;
    sprintf( buf, "http://%s:%d%s", localAddress.toLatin1().data(), QnSoapServer::instance()->port(), QnSoapServer::instance()->path().toLatin1().data() );
    notificationConsumerEndPointAddress.__item = buf;
    notificationConsumerEndPoint.Address = &notificationConsumerEndPointAddress;
    request.ConsumerReference = &notificationConsumerEndPoint;
    //setting InitialTerminationTime (if supported)
    sprintf( buf, "PT%dS", DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT );
    std::string initialTerminationTime( buf );
    request.InitialTerminationTime = &initialTerminationTime;

    //creating filter
    //oasisWsnB2__FilterType topicFilter;
    //strcpy( buf, "<wsnt:TopicExpression Dialect=\"xsd:anyURI\">tns1:Device/Trigger/Relay</wsnt:TopicExpression>" );
    //topicFilter.__any.push_back( buf );
    //request.Filter = &topicFilter;

    _oasisWsnB2__SubscribeResponse response;
    const int soapCallResult = soapWrapper.Subscribe( &request, &response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )    //TODO/IMPL find out which is error and which is not
    {
        NX_LOGX( lit("Failed to subscribe in NotificationProducer. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logWARNING );
        return false;
    }

    NX_LOGX(lit("%1 subscribed to notifications").arg(getUrl()), cl_logDEBUG2);

    if (m_appStopping)
        return false;

    // TODO: #ak if this variable is unused following code may be deleted as well
    time_t utcTerminationTime; // = ::time(NULL) + DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;
    if( response.oasisWsnB2__TerminationTime )
    {
        if( response.oasisWsnB2__CurrentTime )
            utcTerminationTime = ::time(NULL) + *response.oasisWsnB2__TerminationTime - *response.oasisWsnB2__CurrentTime;
        else
            utcTerminationTime = *response.oasisWsnB2__TerminationTime; //hoping local and cam clocks are synchronized
    }
    //else: considering, that onvif device processed initialTerminationTime
    Q_UNUSED(utcTerminationTime)

    std::string subscriptionID;
    if( response.SubscriptionReference )
    {
        if( response.SubscriptionReference->ns1__ReferenceParameters &&
            response.SubscriptionReference->ns1__ReferenceParameters->__item )
        {
            //parsing to retrieve subscriptionId. Example: "<dom0:SubscriptionId xmlns:dom0=\"(null)\">0</dom0:SubscriptionId>"
            QXmlSimpleReader reader;
            SubscriptionReferenceParametersParseHandler handler;
            reader.setContentHandler( &handler );
            QBuffer srcDataBuffer;
            srcDataBuffer.setData(
                response.SubscriptionReference->ns1__ReferenceParameters->__item,
                (int) strlen(response.SubscriptionReference->ns1__ReferenceParameters->__item) );
            QXmlInputSource xmlSrc( &srcDataBuffer );
            if( reader.parse( &xmlSrc ) )
                m_onvifNotificationSubscriptionID = handler.subscriptionID;
        }

        if( response.SubscriptionReference->Address )
            m_onvifNotificationSubscriptionReference = fromOnvifDiscoveredUrl(response.SubscriptionReference->Address->__item);
    }

    //launching renew-subscription timer
    unsigned int renewSubsciptionTimeoutSec = 0;
    if( response.oasisWsnB2__CurrentTime && response.oasisWsnB2__TerminationTime )
        renewSubsciptionTimeoutSec = *response.oasisWsnB2__TerminationTime - *response.oasisWsnB2__CurrentTime;
    else
        renewSubsciptionTimeoutSec = DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;

    scheduleRenewSubscriptionTimer(renewSubsciptionTimeoutSec);

    if (m_appStopping)
        return false;

    /* Note that we don't pass shared pointer here as this would create a
     * cyclic reference and onvif resource will never be deleted. */
    QnSoapServer::instance()->getService()->registerResource(
        toSharedPointer().staticCast<QnPlOnvifResource>(),
        QUrl(QString::fromStdString(m_eventCapabilities->XAddr)).host(),
        m_onvifNotificationSubscriptionReference );

    m_eventMonitorType = emtNotification;

    NX_LOGX( lit("Successfully registered in NotificationProducer. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
    return true;
}

void QnPlOnvifResource::scheduleRenewSubscriptionTimer(unsigned int timeoutSec)
{
    if (timeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS)
        timeoutSec -= RENEW_NOTIFICATION_FORWARDING_SECS;

    const std::chrono::seconds timeout(timeoutSec);
    NX_LOGX(lm("Schedule renew subscription in %1").arg(timeout), cl_logDEBUG2);
    updateTimer(&m_renewSubscriptionTimerID, timeout,
        std::bind(&QnPlOnvifResource::onRenewSubscriptionTimer, this, std::placeholders::_1));
}

CameraDiagnostics::Result QnPlOnvifResource::updateVEncoderUsage(QList<VideoOptionsLocal>& optionsList)
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);

    ProfilesReq request;
    ProfilesResp response;

    int soapRes = soapWrapper.getProfiles(request, response);
    if (soapRes == SOAP_OK)
    {
        for (auto iter = response.Profiles.begin(); iter != response.Profiles.end(); ++iter)
        {
            Profile* profile = *iter;
            if (profile->token.empty() || !profile->VideoEncoderConfiguration)
                continue;
            QString vEncoderID = QString::fromStdString(profile->VideoEncoderConfiguration->token);
            for (int i = 0; i < optionsList.size(); ++i) {
                if (optionsList[i].id == vEncoderID)
                {
                    optionsList[i].usedInProfiles = true;
                    optionsList[i].currentProfile = QString::fromStdString(profile->Name);
                }
            }
        }
        return CameraDiagnostics::NoErrorResult();
    }
    else {
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getProfiles"), soapWrapper.getLastError());
    }
}

bool QnPlOnvifResource::checkResultAndSetStatus(const CameraDiagnostics::Result& result)
{
    bool notAuthorized = result.errorCode == CameraDiagnostics::ErrorCode::notAuthorised;
    if (notAuthorized && getStatus() != Qn::Unauthorized)
        setStatus(Qn::Unauthorized);

    return !!result;
}

bool QnPlOnvifResource::trustMaxFPS()
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool result = resourceData.value<bool>(QString("trustMaxFPS"), false);
    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::getVideoEncoderTokens(MediaSoapWrapper& soapWrapper, QStringList* result, VideoConfigsResp *confResponse)
{
    VideoConfigsReq confRequest;
    result->clear();

    int soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, *confResponse); // get encoder list
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't get list of video encoders from camera (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << "). GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
#endif
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurations"), soapWrapper.getLastError());
    }

    if(m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();


    int confRangeStart = 0;
    int confRangeEnd = (int) confResponse->Configurations.size();
    if (m_maxChannels > 1)
    {
        // determine amount encoder configurations per each video source
        confRangeStart = confRangeEnd/m_maxChannels * getChannel();
        confRangeEnd = confRangeStart + confRangeEnd/m_maxChannels;

        if (confRangeEnd > (int) confResponse->Configurations.size()) {
#ifdef PL_ONVIF_DEBUG
            qWarning() << "invalid channel number " << getChannel()+1 << "for camera" << getHostAddress() << "max channels=" << m_maxChannels;
#endif
            return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurationOptions"), soapWrapper.getLastError());
        }
    }

    for (int confNum = confRangeStart; confNum < confRangeEnd; ++confNum)
    {
        onvifXsd__VideoEncoderConfiguration* configuration = confResponse->Configurations[confNum];
        if (configuration)
            result->push_back(QString::fromStdString(configuration->token));
    }

    return CameraDiagnostics::NoErrorResult();
}

QString QnPlOnvifResource::getInputPortNumberFromString(const QString& portName)
{
    QString portIndex;
    bool canBeConvertedToNumber = false;

    if (portName.toLower().startsWith(lit("di")))
    {
        portIndex = portName.mid(2);

        auto portNum = portIndex.toInt(&canBeConvertedToNumber);

        if (canBeConvertedToNumber)
            return QString::number(portNum + 1);
    }
    else if (portName.startsWith("AlarmIn"))
    {
        portIndex = portName.mid(lit("AlarmIn_").length());

        auto portNum = portIndex.toInt(&canBeConvertedToNumber);

        if (canBeConvertedToNumber)
            return QString::number(portNum + 1);
    }

    return QString();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper)
{

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    QnOnvifConfigDataPtr forcedParams = resourceData.value<QnOnvifConfigDataPtr>(QString("forcedOnvifParams"));
    QStringList videoEncodersTokens;
    VideoConfigsResp confResponse;

    auto frameRateBounds = resourceData.value<QnBounds>(Qn::FPS_BOUNDS_PARAM_NAME, QnBounds());

    if (forcedParams && forcedParams->videoEncoders.size() > getChannel())
    {
        videoEncodersTokens = forcedParams->videoEncoders[getChannel()].split(L',');
    }
    else
    {
        auto error = getVideoEncoderTokens(soapWrapper, &videoEncodersTokens, &confResponse);
        if (error.errorCode != CameraDiagnostics::ErrorCode::noError)
            return error;
    }

    QString login = soapWrapper.getLogin();
    QString password = soapWrapper.getPassword();
    std::string endpoint = soapWrapper.getEndpointUrl().toStdString();

    QList<VideoOptionsLocal> optionsList;
    for(const QString& encoderToken: videoEncodersTokens)
    {

        int retryCount = getMaxOnvifRequestTries();
        int soapRes = SOAP_ERR;

        for (;soapRes != SOAP_OK && retryCount >= 0; --retryCount)
        {
            if(m_appStopping)
                return CameraDiagnostics::ServerTerminatedResult();

            VideoOptionsReq optRequest;
            VideoOptionsResp optResp;
            std::string tokenStdStr = encoderToken.toStdString();
            optRequest.ConfigurationToken = &tokenStdStr;

            MediaSoapWrapper soapWrapper(endpoint, login, password, m_timeDrift);

            soapRes = soapWrapper.getVideoEncoderConfigurationOptions(optRequest, optResp); // get options per encoder
            if (soapRes != SOAP_OK || !optResp.Options)
            {
#ifdef PL_ONVIF_DEBUG
                qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't receive options (or data is empty) for video encoder '"
                    << QString::fromStdString(*(optRequest.ConfigurationToken)) << "' from camera (URL: "  << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
                    << "). Root cause: SOAP request failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();

                qWarning() << "camera" << soapWrapper.getEndpointUrl() << "got soap error for configuration" << configuration->Name.c_str() << "skip configuration";
#endif
                continue;
            }

            if (optResp.Options->H264 || optResp.Options->JPEG)
            {
                optionsList << VideoOptionsLocal(encoderToken, optResp, isH264Allowed(), frameRateBounds);
            }
#ifdef PL_ONVIF_DEBUG
            else
                qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: video encoder '" << encoderToken
                    << "' contains no data for H264/JPEG (URL: "  << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << ")." << "Ignoring and use default codec list";
#endif
        }
        if (soapRes != SOAP_OK)
            return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurationOptions"), soapWrapper.getLastError());
    }

    if (optionsList.isEmpty())
    {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: all video options are empty. (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << ").";
#endif
        return CameraDiagnostics::RequestFailedResult(QLatin1String("fetchAndSetVideoEncoderOptions"), QLatin1String("no video options"));
    }

    if(m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    CameraDiagnostics::Result result = updateVEncoderUsage(optionsList);
    if (!result)
        return result;

    auto profiles = forcedParams ? forcedParams->profiles : QVector<QString>();
    auto channel = getChannel();
    QString channelProfiles;

    if (profiles.size() > channel)
        channelProfiles = profiles[channel];

    auto comparator = createComparator(channelProfiles);
    std::sort(optionsList.begin(), optionsList.end(), comparator);

    /*
    if (optionsList.size() <= m_channelNumer)
    {
        qCritical() << QString(QLatin1String("Not enough encoders for multichannel camera. required at least %1 encoder. URL: %2")).arg(m_channelNumer+1).arg(getUrl());
        return false;
    }
    */

    if (optionsList[0].isH264) {
        m_primaryH264Profile = getH264StreamProfile(optionsList[0]);
        setCodec(H264, true);
    }
    else {
        setCodec(JPEG, true);
    }

    setVideoEncoderOptions(optionsList[0]);
    if (m_maxChannels == 1 && !trustMaxFPS() && !isCameraControlDisabled())
        checkMaxFps(confResponse, optionsList[0].id);

    if(m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    m_mutex.lock();
    m_primaryVideoEncoderId = optionsList[0].id;
    m_secondaryResolutionList = m_resolutionList;
    m_mutex.unlock();

    NX_LOGX(QString(lit("ONVIF debug: got %1 encoders for camera %2")).arg(optionsList.size()).arg(getHostAddress()), cl_logDEBUG1);

    bool dualStreamingAllowed = optionsList.size() >= 2;
    if (dualStreamingAllowed)
    {
        int secondaryIndex = channelProfiles.isEmpty() ? getSecondaryIndex(optionsList) : 1;
        QnMutexLocker lock( &m_mutex );

        m_secondaryVideoEncoderId = optionsList[secondaryIndex].id;
        if (optionsList[secondaryIndex].isH264) {
            m_secondaryH264Profile = getH264StreamProfile(optionsList[secondaryIndex]);
                setCodec(H264, false);
                NX_LOGX(QString(lit("use H264 codec for secondary stream. camera=%1")).arg(getHostAddress()), cl_logDEBUG1);
            }
            else {
                setCodec(JPEG, false);
                NX_LOGX(QString(lit("use JPEG codec for secondary stream. camera=%1")).arg(getHostAddress()), cl_logDEBUG1);
            }
        updateSecondaryResolutionList(optionsList[secondaryIndex]);
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::fetchAndSetDualStreaming(MediaSoapWrapper& /*soapWrapper*/)
{
    QnMutexLocker lock( &m_mutex );

    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));

    bool forceSingleStream = resData.value<bool>(Qn::FORCE_SINGLE_STREAM_PARAM_NAME, false);

    bool dualStreaming =
        !forceSingleStream
        && m_secondaryResolution != EMPTY_RESOLUTION_PAIR
        && !m_secondaryVideoEncoderId.isEmpty();

    if (dualStreaming)
    {
        NX_LOGX(
            lit("ONVIF debug: enable dualstreaming for camera %1")
                .arg(getHostAddress()),
            cl_logDEBUG1);
    }
    else
    {
        QString reason =
            forceSingleStream ?
                lit("single stream mode is forced by driver") :
            m_secondaryResolution == EMPTY_RESOLUTION_PAIR ?
                lit("no secondary resolution") :
                QLatin1String("no secondary encoder");

        NX_LOGX(
            lit("ONVIF debug: disable dualstreaming for camera %1 reason: %2")
                .arg(getHostAddress())
                .arg(reason),
            cl_logDEBUG1);
    }

    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, dualStreaming ? 1 : 0);
    return true;
}

bool QnPlOnvifResource::fetchAndSetAudioEncoderOptions(MediaSoapWrapper& soapWrapper)
{
    AudioOptionsReq request;
    std::string token = m_audioEncoderId.toStdString();
    request.ConfigurationToken = &token;
    request.ProfileToken = NULL;

    AudioOptionsResp response;

    int soapRes = soapWrapper.getAudioEncoderConfigurationOptions(request, response);
    if (soapRes != SOAP_OK || !response.Options) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return false;

    }

    AUDIO_CODECS codec = AUDIO_NONE;
    AudioOptions* options = NULL;

    std::vector<AudioOptions*>::const_iterator it = response.Options->Options.begin();

    while (it != response.Options->Options.end()) {

        AudioOptions* curOpts = *it;
        if (curOpts)
        {
            switch (curOpts->Encoding)
            {
                case onvifXsd__AudioEncoding__G711:
                    if (codec < G711) {
                        codec = G711;
                        options = curOpts;
                    }
                    break;
                case onvifXsd__AudioEncoding__G726:
                    if (codec < G726) {
                        codec = G726;
                        options = curOpts;
                    }
                    break;
                case onvifXsd__AudioEncoding__AAC:
                    if (codec < AAC) {
                        codec = AAC;
                        options = curOpts;
                    }
                    break;
                case onvifXsd__AudioEncoding__AMR:
                    if (codec < AMR) {
                        codec = AMR;
                        options = curOpts;
                    }
                    break;
                default:
#ifdef PL_ONVIF_DEBUG
                    qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: got unknown codec type: "
                        << curOpts->Encoding << " (URL: " << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
                        << "). Root cause: SOAP request failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
#endif
                    break;
            }
        }

        ++it;
    }

    if (!options) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return data for G711, G726 or ACC (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return false;

    }


    setAudioCodec(codec);

    setAudioEncoderOptions(*options);

    return true;
}

void QnPlOnvifResource::setAudioEncoderOptions(const AudioOptions& options)
{
    int bitRate = 0;
    if (options.BitrateList)
    {
        bitRate = findClosestRateFloor(options.BitrateList->Items, MAX_AUDIO_BITRATE);
    }
#ifdef PL_ONVIF_DEBUG
    else
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return Bitrate List ( UniqueId: "
            << getUniqueId() << ").";
    }
#endif

    int sampleRate = 0;
    if (options.SampleRateList)
    {
        sampleRate = findClosestRateFloor(options.SampleRateList->Items, MAX_AUDIO_SAMPLERATE);
    }
#ifdef PL_ONVIF_DEBUG
    else
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return Samplerate List ( UniqueId: "
            << getUniqueId() << ").";
    }
#endif

    {
        QnMutexLocker lock( &m_mutex );
        m_audioSamplerate = sampleRate;
        m_audioBitrate = bitRate;
    }
}

int QnPlOnvifResource::findClosestRateFloor(const std::vector<int>& values, int threshold) const
{
    int floor = threshold;
    int ceil = threshold;

    std::vector<int>::const_iterator it = values.begin();

    while (it != values.end())
    {
        if (*it == threshold) {
            return *it;
        }

        if (*it < threshold && *it > floor) {
            floor = *it;
        } else if (*it > threshold && *it < ceil) {
            ceil = *it;
        }

        ++it;
    }

    if (floor < threshold) {
        return floor;
    }

    if (ceil > threshold) {
        return ceil;
    }

    return 0;
}

CameraDiagnostics::Result QnPlOnvifResource::updateResourceCapabilities()
{
    QnMutexLocker lock( &m_mutex );

    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));

    if (!m_videoSourceSize.isValid())
        return CameraDiagnostics::NoErrorResult();


    NX_LOGX(QString(lit("ONVIF debug: videoSourceSize is %1x%2 for camera %3")).
        arg(m_videoSourceSize.width()).arg(m_videoSourceSize.height()).arg(getHostAddress()), cl_logDEBUG1);

    bool trustToVideoSourceSize = false;
    for (const auto& resolution: m_resolutionList)
    {
        if (resolution.width() <= m_videoSourceSize.width() && resolution.height() <= m_videoSourceSize.height())
            trustToVideoSourceSize = true; // trust to videoSourceSize if at least 1 appropriate resolution is exists.

    }

    bool videoSourceSizeIsRight = resData.value<bool>(Qn::TRUST_TO_VIDEO_SOURCE_SIZE_PARAM_NAME, true);
    if (!videoSourceSizeIsRight)
        trustToVideoSourceSize = false;

    if (!trustToVideoSourceSize)
    {
        NX_LOGX(QString(lit("ONVIF debug: do not trust to videoSourceSize is %1x%2 for camera %3 because it blocks all resolutions")).
            arg(m_videoSourceSize.width()).arg(m_videoSourceSize.height()).arg(getHostAddress()), cl_logDEBUG1);
        return CameraDiagnostics::NoErrorResult();
    }


    QList<QSize>::iterator it = m_resolutionList.begin();
    while (it != m_resolutionList.end())
    {
        if (it->width() > m_videoSourceSize.width() || it->height() > m_videoSourceSize.height())
        {

            NX_LOGX(QString(lit("ONVIF debug: drop resolution %1x%2 for camera %3 because resolution > videoSourceSize")).
                arg(it->width()).arg(it->width()).arg(getHostAddress()), cl_logDEBUG1);

            it = m_resolutionList.erase(it);
        }
        else
        {
            return CameraDiagnostics::NoErrorResult();
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

int QnPlOnvifResource::getGovLength() const
{
    QnMutexLocker lock( &m_mutex );

    return m_iframeDistance;
}

bool QnPlOnvifResource::fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper)
{
    AudioConfigsReq request;
    AudioConfigsResp response;

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoder: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return false;

    }

    if (response.Configurations.empty()) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoder: empty data received from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return false;
    } else {
        if ((int)response.Configurations.size() > getChannel())
        {
            onvifXsd__AudioEncoderConfiguration* conf = response.Configurations.at(getChannel());
        if (conf) {
            QnMutexLocker lock( &m_mutex );
            // TODO: #vasilenko UTF unuse std::string
            m_audioEncoderId = QString::fromStdString(conf->token);
        }
    }
#ifdef PL_ONVIF_DEBUG
        else {
            qWarning() << "Can't find appropriate audio encoder. url=" << getUrl();
            return false;
        }
#endif
    }

    return true;
}

void QnPlOnvifResource::updateVideoSource(VideoSource* source, const QRect& maxRect) const
{
    //One name for primary and secondary
    //source.Name = NETOPTIX_PRIMARY_NAME;

    if (!source->Bounds) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::updateVideoSource: rectangle object is NULL. UniqueId: " << getUniqueId();
#endif
        return;
    }

    if (!m_videoSourceSize.isValid())
        return;

    source->Bounds->x = maxRect.left();
    source->Bounds->y = maxRect.top();
    source->Bounds->width = maxRect.width();
    source->Bounds->height = maxRect.height();
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoSourceToCamera(VideoSource* source)
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), getTimeDrift());

    SetVideoSrcConfigReq request;
    SetVideoSrcConfigResp response;
    request.Configuration = source;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::setVideoSourceConfiguration: can't set required values into ONVIF physical device (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
#endif

        if (soapWrapper.isNotAuthenticated())
            setStatus(Qn::Unauthorized);

        return CameraDiagnostics::NoErrorResult(); // ignore error because of some cameras is not ONVIF profile S compatible and doesn't support this request
        //return CameraDiagnostics::RequestFailedResult(QLatin1String("setVideoSourceConfiguration"), soapWrapper.getLastError());
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::detectVideoSourceCount()
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);

    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return false;
    }
    m_maxChannels = (int) response.VideoSources.size();

    if (m_maxChannels > 1)
    {
        VideoConfigsReq confRequest;
        VideoConfigsResp confResponse;
        soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse); // get encoder list
        if (soapRes != SOAP_OK)
            return false;
        int encoderCount = (int) confResponse.Configurations.size();
        if (encoderCount > 0 && encoderCount < m_maxChannels)
            m_maxChannels = encoderCount;
    }

    return true;
}

CameraDiagnostics::Result QnPlOnvifResource::fetchVideoSourceToken()
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);

    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        if (soapWrapper.isNotAuthenticated())
        {
            setStatus(Qn::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult( getMediaUrl() );
        }
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSources"), soapWrapper.getLastError());

    }

    m_maxChannels = (int) response.VideoSources.size();

    if (m_maxChannels <= getChannel()) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: empty data received from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSources"), QLatin1String("missing video source configuration (1)"));
    }

    onvifXsd__VideoSource* conf = response.VideoSources.at(getChannel());

    if (!conf)
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSources"), QLatin1String("missing video source configuration (2)"));

    QnMutexLocker lock( &m_mutex );
    m_videoSourceToken = QString::fromStdString(conf->token);
    //m_videoSourceSize = QSize(conf->Resolution->Width, conf->Resolution->Height);

    if (m_maxChannels > 1)
    {
        VideoConfigsReq confRequest;
        VideoConfigsResp confResponse;
        soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse); // get encoder list
        if (soapRes != SOAP_OK)
            return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurations"), soapWrapper.getLastError());

        int encoderCount = (int)confResponse.Configurations.size();
        if (encoderCount > 0 && encoderCount < m_maxChannels)
            m_maxChannels = encoderCount;
    }

    return CameraDiagnostics::NoErrorResult();
}

QRect QnPlOnvifResource::getVideoSourceMaxSize(const QString& configToken)
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    VideoSrcOptionsReq request;
    std::string token = configToken.toStdString();
    request.ConfigurationToken = &token;
    request.ProfileToken = NULL;

    VideoSrcOptionsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurationOptions(request, response);

    bool isValid = response.Options
        && response.Options->BoundsRange
        && response.Options->BoundsRange->XRange
        && response.Options->BoundsRange->YRange
        && response.Options->BoundsRange->WidthRange
        && response.Options->BoundsRange->HeightRange;

    if (soapRes != SOAP_OK || !isValid) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSourceOptions: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return QRect();
    }
    onvifXsd__IntRectangleRange* br = response.Options->BoundsRange;
    QRect result(qMax(0, br->XRange->Min), qMax(0, br->YRange->Min), br->WidthRange->Max, br->HeightRange->Max);
    if (result.isEmpty())
        return QRect();
    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoSource()
{
    CameraDiagnostics::Result result = fetchVideoSourceToken();
    if (!result)
        return result;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    VideoSrcConfigsReq request;
    VideoSrcConfigsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSourceConfigurations"), soapWrapper.getLastError());

    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    std::string srcToken = m_videoSourceToken.toStdString();
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        onvifXsd__VideoSourceConfiguration* conf = response.Configurations.at(i);
        if (!conf || conf->SourceToken != srcToken || !(conf->Bounds))
            continue;

        {
            QnMutexLocker lock( &m_mutex );
            m_videoSourceId = QString::fromStdString(conf->token);
        }

        QRect currentRect(conf->Bounds->x, conf->Bounds->y, conf->Bounds->width, conf->Bounds->height);
        QRect maxRect = getVideoSourceMaxSize(QString::fromStdString(conf->token));
        if (maxRect.isValid())
            m_videoSourceSize = QSize(maxRect.width(), maxRect.height());
        if (maxRect.isValid() && currentRect != maxRect && !isCameraControlDisabled())
        {
            updateVideoSource(conf, maxRect);
            return sendVideoSourceToCamera(conf);
        }
        else {
            return CameraDiagnostics::NoErrorResult();
        }

        if (m_appStopping)
            return CameraDiagnostics::ServerTerminatedResult();
    }

    return CameraDiagnostics::UnknownErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetAudioSource()
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    AudioSrcConfigsReq request;
    AudioSrcConfigsResp response;

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getAudioSourceConfigurations"), soapWrapper.getLastError());

    }

    if ((int)response.Configurations.size() <= getChannel()) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: empty data received from camera (or data is empty) (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
#endif
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getAudioSourceConfigurations"), QLatin1String("missing channel configuration (1)"));
    } else {
        onvifXsd__AudioSourceConfiguration* conf = response.Configurations.at(getChannel());
        if (conf) {
            QnMutexLocker lock( &m_mutex );
            // TODO: #vasilenko UTF unuse std::string
            m_audioSourceId = QString::fromStdString(conf->token);
            return CameraDiagnostics::NoErrorResult();
        }
    }

    return CameraDiagnostics::RequestFailedResult(QLatin1String("getAudioSourceConfigurations"), QLatin1String("missing channel configuration (2)"));
}

QnConstResourceAudioLayoutPtr QnPlOnvifResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (isAudioEnabled()) {
        const QnOnvifStreamReader* onvifReader = dynamic_cast<const QnOnvifStreamReader*>(dataProvider);
        if (onvifReader && onvifReader->getDPAudioLayout())
            return onvifReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

bool QnPlOnvifResource::loadAdvancedParamsUnderLock(QnCameraAdvancedParamValueMap &values) {
    m_prevOnvifResultCode = CameraDiagnostics::NoErrorResult();

    if (!m_imagingParamsProxy) {
        m_prevOnvifResultCode = CameraDiagnostics::UnknownErrorResult();
        return false;
    }

    m_prevOnvifResultCode = m_imagingParamsProxy->loadValues(values);
    return m_prevOnvifResultCode.errorCode == CameraDiagnostics::ErrorCode::noError;
}

bool QnPlOnvifResource::getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList& result)
{
    if (m_appStopping)
        return false;

    QnMutexLocker lock( &m_physicalParamsMutex );
    m_advancedParamsCache.clear();
    if (loadAdvancedParamsUnderLock(m_advancedParamsCache)) {
        m_advSettingsLastUpdated.restart();
    }
    else {
        m_advSettingsLastUpdated.invalidate();
        return false;
    }
    bool success = true;
    for(const QString &id: idList) {
        if (m_advancedParamsCache.contains(id))
            result << QnCameraAdvancedParamValue(id, m_advancedParamsCache[id]);
        else
            success = false;
    }

    return success;
}

bool QnPlOnvifResource::getParamPhysical(const QString &id, QString &value) {
    if (m_appStopping)
        return false;

    //Caching camera values during ADVANCED_SETTINGS_VALID_TIME to avoid multiple excessive 'get' requests to camera
    QnMutexLocker lock( &m_physicalParamsMutex );
    if (!m_advSettingsLastUpdated.isValid() || m_advSettingsLastUpdated.hasExpired(ADVANCED_SETTINGS_VALID_TIME * 1000)) {
        m_advancedParamsCache.clear();
        if (loadAdvancedParamsUnderLock(m_advancedParamsCache))
            m_advSettingsLastUpdated.restart();
    }

    if (!m_advancedParamsCache.contains(id))
        return false;
    value = m_advancedParamsCache[id];
    return true;
}

bool QnPlOnvifResource::setAdvancedParameterUnderLock(const QnCameraAdvancedParameter &parameter, const QString &value) {
    if (m_imagingParamsProxy && m_imagingParamsProxy->supportedParameters().contains(parameter.id))
        return m_imagingParamsProxy->setValue(parameter.id, value);

    if (m_maintenanceProxy && m_maintenanceProxy->supportedParameters().contains(parameter.id))
        return m_maintenanceProxy->callOperation(parameter.id);

    return false;
}

bool QnPlOnvifResource::setAdvancedParametersUnderLock(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    bool success = true;
    for(const QnCameraAdvancedParamValue &value: values)
    {
        QnCameraAdvancedParameter parameter = m_advancedParameters.getParameterById(value.id);
        if (parameter.isValid() && setAdvancedParameterUnderLock(parameter, value.value))
            result << value;
        else
            success = false;
    }
    return success;
}


//positive number means timeout in seconds
//negative number - timeout in milliseconds
void QnPlOnvifResource::setOnvifRequestsRecieveTimeout(int timeout)
{
    m_onvifRecieveTimeout = timeout;
}

void QnPlOnvifResource::setOnvifRequestsSendTimeout(int timeout)
{
    m_onvifSendTimeout = timeout;
}

int QnPlOnvifResource::getOnvifRequestsRecieveTimeout() const
{
    if (m_onvifRecieveTimeout)
        return m_onvifRecieveTimeout;

    return DEFAULT_SOAP_TIMEOUT;
}

int QnPlOnvifResource::getOnvifRequestsSendTimeout() const
{
    if (m_onvifSendTimeout)
        return m_onvifSendTimeout;

    return DEFAULT_SOAP_TIMEOUT;
}

bool QnPlOnvifResource::setParamsPhysical(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    bool success;
    {
        setParamsBegin();
        QnMutexLocker lock( &m_physicalParamsMutex );
        success = setAdvancedParametersUnderLock(values, result);
        for (const auto& updatedValue: result)
            m_advancedParamsCache[updatedValue.id] = updatedValue.value;
        setParamsEnd();
    }
    for (const auto& updatedValue: result)
        emit advancedParameterChanged(updatedValue.id, updatedValue.value);
    return success;
}

bool QnPlOnvifResource::setParamPhysical(const QString &id, const QString& value) {
    if (m_appStopping)
        return false;

    bool result = false;
    {
        QnMutexLocker lock( &m_physicalParamsMutex );
        //if (!m_advancedParamsCache.contains(id))
        //    return false; // there are no write-only parameters in a cache

        QnCameraAdvancedParameter parameter = m_advancedParameters.getParameterById(id);
        if (!parameter.isValid())
            return false;

        result = setAdvancedParameterUnderLock(parameter, value);
        if (result)
            m_advancedParamsCache[id] = value;
    }
    if (result)
        emit advancedParameterChanged(id, value);
    return result;
}

bool QnPlOnvifResource::loadAdvancedParametersTemplate(QnCameraAdvancedParams &params) const
{
    return loadXmlParametersInternal(params, lit(":/camera_advanced_params/onvif.xml"));
}

bool QnPlOnvifResource::loadXmlParametersInternal(QnCameraAdvancedParams &params, const QString& paramsTemplateFileName) const
{
    QFile paramsTemplateFile(paramsTemplateFileName);
#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif
    bool result = QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);

    if (!result)
    {
        NX_LOGX(lit("Error while parsing xml (onvif) %1").arg(paramsTemplateFileName), cl_logWARNING);
    }


    return result;
}

void QnPlOnvifResource::initAdvancedParametersProviders(QnCameraAdvancedParams &params) {
    QAuthenticator auth = getAuth();
    QString imagingUrl = getImagingUrl();
    if (!imagingUrl.isEmpty()) {
        m_imagingParamsProxy.reset(new QnOnvifImagingProxy(imagingUrl.toLatin1().data(),  auth.user(), auth.password(), m_videoSourceToken.toStdString(), m_timeDrift) );
        m_imagingParamsProxy->initParameters(params);
    }

    QString maintenanceUrl = getDeviceOnvifUrl();
    if (!maintenanceUrl.isEmpty()) {
        m_maintenanceProxy.reset(new QnOnvifMaintenanceProxy(maintenanceUrl, auth, m_videoSourceToken, m_timeDrift));
    }
}

QSet<QString> QnPlOnvifResource::calculateSupportedAdvancedParameters() const {
    QSet<QString> result;
    if (m_imagingParamsProxy)
        result.unite(m_imagingParamsProxy->supportedParameters());
    if (m_maintenanceProxy)
        result.unite(m_maintenanceProxy->supportedParameters());
    return result;
}

void QnPlOnvifResource::fetchAndSetAdvancedParameters() {
    QnMutexLocker lock( &m_physicalParamsMutex );
    m_advancedParameters.clear();

    QnCameraAdvancedParams params;
    if (!loadAdvancedParametersTemplate(params))
        return;

    initAdvancedParametersProviders(params);

    QSet<QString> supportedParams = calculateSupportedAdvancedParameters();
    m_advancedParameters = params.filtered(supportedParams);
    QnCameraAdvancedParamsReader::setParamsToResource(this->toSharedPointer(), m_advancedParameters);
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoEncoderToCamera(VideoEncoder& encoder)
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);

    auto proxy = soapWrapper.getProxy();
    proxy->soap->recv_timeout = getOnvifRequestsRecieveTimeout();
    proxy->soap->send_timeout = getOnvifRequestsSendTimeout();

    SetVideoConfigReq request;
    SetVideoConfigResp response;
    request.Configuration = &encoder;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK)
    {
        if (soapWrapper.isNotAuthenticated())
            setStatus(Qn::Unauthorized);

#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::sendVideoEncoderToCamera: can't set required values into ONVIF physical device (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
#endif
        if (soapWrapper.getLastError().contains(QLatin1String("not possible to set")))
            return CameraDiagnostics::CannotConfigureMediaStreamResult( QLatin1String("fps") );   // TODO: #ak find param name
        else
            return CameraDiagnostics::CannotConfigureMediaStreamResult( QString() );
    }
    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::onRenewSubscriptionTimer(quint64 timerID)
{
    QnMutexLocker lk( &m_ioPortMutex );

    if( !m_eventCapabilities.get() )
        return;
    if( timerID != m_renewSubscriptionTimerID )
        return;
    m_renewSubscriptionTimerID = 0;

    QAuthenticator auth = getAuth();
    SubscriptionManagerSoapWrapper soapWrapper(
        m_onvifNotificationSubscriptionReference.isEmpty()
            ? m_eventCapabilities->XAddr
            : m_onvifNotificationSubscriptionReference.toLatin1().constData(),
        auth.user(),
        auth.password(),
        m_timeDrift );
    soapWrapper.getProxy()->soap->imode |= SOAP_XML_IGNORENS;

    char buf[256];

    _oasisWsnB2__Renew request;
    sprintf( buf, "PT%dS", DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT );
    std::string initialTerminationTime = buf;
    request.TerminationTime = &initialTerminationTime;
    if( !m_onvifNotificationSubscriptionID.isEmpty() )
    {
        sprintf( buf, "<dom0:SubscriptionId xmlns:dom0=\"http://www.onvifplus.org/event\">%s</dom0:SubscriptionId>", m_onvifNotificationSubscriptionID.toLatin1().data() );
        request.__any.push_back( buf );
    }
    _oasisWsnB2__RenewResponse response;
    //NOTE: renewing session does not work on vista. Should ignore error in that case
    const int soapCallResult = soapWrapper.renew( request, response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        if( m_eventCapabilities && m_eventCapabilities->WSPullPointSupport )
        {
            // Ignoring renew error since it does not work on some cameras (on Vista, particularly).
            NX_LOGX( lit("Ignoring renew error on %1").arg(getUrl()), cl_logDEBUG2 );
        }
        else
        {
            NX_LOGX( lit("Failed to renew subscription (endpoint %1). %2").
                arg(QString::fromLatin1(soapWrapper.endpoint())).arg(soapCallResult), cl_logDEBUG1 );
            lk.unlock();

            _oasisWsnB2__Unsubscribe request;
            _oasisWsnB2__UnsubscribeResponse response;
            soapWrapper.unsubscribe(request, response);

            QnSoapServer::instance()->getService()->removeResourceRegistration( toSharedPointer().staticCast<QnPlOnvifResource>() );
            if( !registerNotificationConsumer() )
            {
                lk.relock();
                scheduleRetrySubscriptionTimer();
            }
            return;
        }
    }
    else
    {
        NX_LOGX( lit("Renewed subscription to %1").arg(getUrl()), cl_logDEBUG2 );
    }

    unsigned int renewSubsciptionTimeoutSec = response.oasisWsnB2__CurrentTime
        ? (response.oasisWsnB2__TerminationTime - *response.oasisWsnB2__CurrentTime)
        : DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;

    scheduleRenewSubscriptionTimer( renewSubsciptionTimeoutSec );
}

void QnPlOnvifResource::checkMaxFps(VideoConfigsResp& response, const QString& encoderId)
{
    VideoEncoder* vEncoder = 0;
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        auto configuration = response.Configurations[i];
        if (configuration && QString::fromStdString(configuration->token) == encoderId)
            vEncoder = configuration;
    }
    if (!vEncoder || !vEncoder->RateControl)
        return;

    int maxFpsOrig = getMaxFps();
    int rangeHi = getMaxFps()-2;
    int rangeLow = getMaxFps()/4;
    int currentFps = rangeHi;
    int prevFpsValue = -1;

    m_mutex.lock();
    vEncoder->Resolution->Width = m_resolutionList[0].width();
    vEncoder->Resolution->Height = m_resolutionList[0].height();
    m_mutex.unlock();

    while (currentFps != prevFpsValue)
    {
        vEncoder->RateControl->FrameRateLimit = currentFps;
        bool success = false;
        bool invalidFpsDetected = false;
        for (int i = 0; i < getMaxOnvifRequestTries(); ++i)
        {
            if(m_appStopping)
                return;

            vEncoder->RateControl->FrameRateLimit = currentFps;
            CameraDiagnostics::Result result = sendVideoEncoderToCamera(*vEncoder);
            if (result.errorCode == CameraDiagnostics::ErrorCode::noError)
            {
                if (currentFps >= maxFpsOrig-2) {
                    // If first try success, does not change maxFps at all. (HikVision has working range 0..15, and 25 fps, so try from max-1 checking)
                    return;
                }
                setMaxFps(currentFps);
                success = true;
                break;
            }
            else if (result.errorCode == CameraDiagnostics::ErrorCode::cannotConfigureMediaStream &&
                     result.errorParams.indexOf(QLatin1String("fps")) != -1 )
            {
                invalidFpsDetected = true;
                break; // invalid fps
            }
        }
        if (!invalidFpsDetected && !success)
        {
            // can't determine fps (cameras does not answer e.t.c)
            setMaxFps(maxFpsOrig);
            return;
        }

        prevFpsValue = currentFps;
        if (success) {
            rangeLow = currentFps;
            currentFps += (rangeHi-currentFps+1)/2;
        }
        else {
            rangeHi = currentFps-1;
            currentFps -= (currentFps-rangeLow+1)/2;
        }
    }
}

QnAbstractPtzController* QnPlOnvifResource::createSpecialPtzController()
{
    if (getModel() == lit("DCS-5615"))
        return new QnDlinkPtzController(toSharedPointer(this));
    else
        return 0;
}

QnAbstractPtzController *QnPlOnvifResource::createPtzControllerInternal()
{
    QScopedPointer<QnAbstractPtzController> result;
    result.reset(createSpecialPtzController());
    if (result)
        return result.take();

    if(getPtzUrl().isEmpty() || getPtzConfigurationToken().isEmpty())
        return NULL;

    result.reset(new QnOnvifPtzController(toSharedPointer(this)));
    if(result->getCapabilities() == Ptz::NoPtzCapabilities)
        return NULL;

    return result.take();
}

bool QnPlOnvifResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
{
    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        !hasCameraCapabilities(Qn::RelayInputCapability) )
    {
        return false;
    }

    if( !m_eventCapabilities.get() )
        return false;

    {
        QnMutexLocker lk( &m_ioPortMutex );
        NX_ASSERT( !m_inputMonitored );
        m_inputMonitored = true;
    }

    const auto result = subscribeToCameraNotifications();
    NX_LOGX(lit("Port monitoring has started: %1").arg(result), cl_logDEBUG1);
    return result;
}

void QnPlOnvifResource::scheduleRetrySubscriptionTimer()
{
    static const std::chrono::seconds kTimeout(
        DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT);

    NX_LOGX(lm("Schedule new subscription in %1").arg(kTimeout), cl_logDEBUG2);
    updateTimer(&m_renewSubscriptionTimerID, kTimeout,
        [this](quint64 timerId)
        {
            QnMutexLocker lock(&m_ioPortMutex);
            if (timerId != m_renewSubscriptionTimerID)
                return;

            bool isSubscribed = false;
            {
                QnMutexUnlocker unlock(&lock);
                isSubscribed = createPullPointSubscription();
            }

            if (isSubscribed)
                scheduleRetrySubscriptionTimer();
            else
                m_renewSubscriptionTimerID = 0;
        });
}

bool QnPlOnvifResource::subscribeToCameraNotifications()
{
    if( m_eventCapabilities->WSPullPointSupport )
        return createPullPointSubscription();
    else if( QnSoapServer::instance()->initialized() )
        return registerNotificationConsumer();
    else
        return false;
}

void QnPlOnvifResource::stopInputPortMonitoringAsync()
{
    //TODO #ak this method MUST become asynchronous
    quint64 nextPullMessagesTimerIDBak = 0;
    quint64 renewSubscriptionTimerIDBak = 0;
    {
        QnMutexLocker lk( &m_ioPortMutex );
        m_inputMonitored = false;
        nextPullMessagesTimerIDBak = m_nextPullMessagesTimerID;
        m_nextPullMessagesTimerID = 0;
        renewSubscriptionTimerIDBak = m_renewSubscriptionTimerID;
        m_renewSubscriptionTimerID = 0;
    }

    //removing timer
    if( nextPullMessagesTimerIDBak > 0 )
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(nextPullMessagesTimerIDBak);
    if( renewSubscriptionTimerIDBak > 0 )
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(renewSubscriptionTimerIDBak);
    //TODO #ak removing device event registration
        //if we do not remove event registration, camera will do it for us in some timeout

    QSharedPointer<GSoapAsyncPullMessagesCallWrapper> asyncPullMessagesCallWrapper;
    {
        QnMutexLocker lk( &m_ioPortMutex );
        std::swap(asyncPullMessagesCallWrapper, m_asyncPullMessagesCallWrapper);
    }

    if( asyncPullMessagesCallWrapper )
    {
        asyncPullMessagesCallWrapper->pleaseStop();
        asyncPullMessagesCallWrapper->join();
    }

    if (QnSoapServer::instance() && QnSoapServer::instance()->getService())
        QnSoapServer::instance()->getService()->removeResourceRegistration( toSharedPointer().staticCast<QnPlOnvifResource>() );

    NX_LOGX(lit("Port monitoring is stopped"), cl_logDEBUG1);
}


//////////////////////////////////////////////////////////
// QnPlOnvifResource::SubscriptionReferenceParametersParseHandler
//////////////////////////////////////////////////////////

QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::SubscriptionReferenceParametersParseHandler()
:
    m_readingSubscriptionID( false )
{
}

bool QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::characters( const QString& ch )
{
    if( m_readingSubscriptionID )
        subscriptionID = ch;
    return true;
}

bool QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::startElement(
    const QString& /*namespaceURI*/,
    const QString& localName,
    const QString& /*qName*/,
    const QXmlAttributes& /*atts*/ )
{
    if( localName == QLatin1String("SubscriptionId") )
        m_readingSubscriptionID = true;
    return true;
}

bool QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::endElement(
    const QString& /*namespaceURI*/,
    const QString& localName,
    const QString& /*qName*/ )
{
    if( localName == QLatin1String("SubscriptionId") )
        m_readingSubscriptionID = false;
    return true;
}


//////////////////////////////////////////////////////////
// QnPlOnvifResource::NotificationMessageParseHandler
//////////////////////////////////////////////////////////

QnPlOnvifResource::NotificationMessageParseHandler::NotificationMessageParseHandler(QTimeZone timeZone):
    timeZone(timeZone)
{
    m_parseStateStack.push( init );
}

bool QnPlOnvifResource::NotificationMessageParseHandler::startElement(
    const QString& /*namespaceURI*/,
    const QString& localName,
    const QString& /*qName*/,
    const QXmlAttributes& atts )
{
    switch( m_parseStateStack.top() )
    {
        case init:
        {
            if( localName != lit("Message") )
                return false;
            int utcTimeIndex = atts.index( lit("UtcTime") );
            if( utcTimeIndex == -1 )
                return false;   //missing required attribute

            utcTime = QDateTime::fromString( atts.value(utcTimeIndex), Qt::ISODate );
            if( utcTime.timeSpec() == Qt::LocalTime )
                utcTime.setTimeZone( timeZone );
            if( utcTime.timeSpec() != Qt::UTC )
                utcTime = utcTime.toUTC();

            propertyOperation = atts.value( lit("PropertyOperation") );
            m_parseStateStack.push( readingMessage );
            break;
        }

        case readingMessage:
        {
            if( localName == QLatin1String("Source") )
                m_parseStateStack.push( readingSource );
            else if( localName == QLatin1String("Data") )
                m_parseStateStack.push( readingData );
            else
                m_parseStateStack.push( skipping );
            break;
        }

        case readingSource:
        {
            if( localName != QLatin1String("SimpleItem") )
                return false;
            int nameIndex = atts.index( QLatin1String("Name") );
            if( nameIndex == -1 )
                return false;   //missing required attribute
            int valueIndex = atts.index( QLatin1String("Value") );
            if( valueIndex == -1 )
                return false;   //missing required attribute
            source.push_back( SimpleItem( atts.value(nameIndex), atts.value(valueIndex) ) );
            m_parseStateStack.push( readingSourceItem );
            break;
        }

        case readingSourceItem:
            return false;   //unexpected

        case readingData:
        {
            if( localName != QLatin1String("SimpleItem") )
                return false;
            int nameIndex = atts.index( QLatin1String("Name") );
            if( nameIndex == -1 )
                return false;   //missing required attribute
            int valueIndex = atts.index( QLatin1String("Value") );
            if( valueIndex == -1 )
                return false;   //missing required attribute
            data.name = atts.value(nameIndex);
            data.value = atts.value(valueIndex);
            m_parseStateStack.push( readingDataItem );
            break;
        }

        case readingDataItem:
            return false;   //unexpected

        case skipping:
            m_parseStateStack.push( skipping );

        default:
            return false;
    }

    return true;
}

bool QnPlOnvifResource::NotificationMessageParseHandler::endElement(
    const QString& /*namespaceURI*/,
    const QString& /*localName*/,
    const QString& /*qName*/ )
{
    if( m_parseStateStack.empty() )
        return false;
    m_parseStateStack.pop();
    return true;
}

//////////////////////////////////////////////////////////
// QnPlOnvifResource
//////////////////////////////////////////////////////////

bool QnPlOnvifResource::createPullPointSubscription()
{
    QAuthenticator auth = getAuth();
    EventSoapWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user(),
        auth.password(),
        m_timeDrift );
    soapWrapper.getProxy()->soap->imode |= SOAP_XML_IGNORENS;

    _onvifEvents__CreatePullPointSubscription request;
    std::string initialTerminationTime = "PT600S";
    request.InitialTerminationTime = &initialTerminationTime;
    _onvifEvents__CreatePullPointSubscriptionResponse response;
    const int soapCallResult = soapWrapper.createPullPointSubscription( request, response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOGX( lm("Failed to subscribe to %1").arg(soapWrapper.endpoint()), cl_logWARNING );
        scheduleRenewSubscriptionTimer(RENEW_NOTIFICATION_FORWARDING_SECS);
        return false;
    }

    NX_LOGX( lm("Successfuly created pool point to %1").arg(soapWrapper.endpoint()), cl_logDEBUG2 );
    std::string subscriptionID;
    if( response.SubscriptionReference )
    {
        if( response.SubscriptionReference->ns1__ReferenceParameters &&
            response.SubscriptionReference->ns1__ReferenceParameters->__item )
        {
            //parsing to retrieve subscriptionId. Example: "<dom0:SubscriptionId xmlns:dom0=\"(null)\">0</dom0:SubscriptionId>"
            QXmlSimpleReader reader;
            SubscriptionReferenceParametersParseHandler handler;
            reader.setContentHandler( &handler );
            QBuffer srcDataBuffer;
            srcDataBuffer.setData(
                response.SubscriptionReference->ns1__ReferenceParameters->__item,
                (int) strlen(response.SubscriptionReference->ns1__ReferenceParameters->__item) );
            QXmlInputSource xmlSrc( &srcDataBuffer );
            if( reader.parse( &xmlSrc ) )
                m_onvifNotificationSubscriptionID = handler.subscriptionID;
        }

        if( response.SubscriptionReference->Address )
            m_onvifNotificationSubscriptionReference = fromOnvifDiscoveredUrl(response.SubscriptionReference->Address->__item);
    }

    //adding task to refresh subscription
    unsigned int renewSubsciptionTimeoutSec = response.oasisWsnB2__TerminationTime - response.oasisWsnB2__CurrentTime;

    QnMutexLocker lk( &m_ioPortMutex );

    if( !m_inputMonitored )
        return true;

    if( qnStaticCommon->dataPool()->data(toSharedPointer(this)).value<bool>(lit("renewOnvifPullPointSubscriptionRequired"), true) )
    {
        // NOTE: Renewing session does not work on vista.
        scheduleRenewSubscriptionTimer(renewSubsciptionTimeoutSec);
    }

    m_eventMonitorType = emtPullPoint;
    m_prevPullMessageResponseClock = m_monotonicClock.elapsed();

    updateTimer( &m_nextPullMessagesTimerID,
        std::chrono::milliseconds(PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC * MS_PER_SECOND),
        std::bind(&QnPlOnvifResource::pullMessages, this, std::placeholders::_1) );
    return true;
}

void QnPlOnvifResource::removePullPointSubscription()
{
    QAuthenticator auth = getAuth();
    SubscriptionManagerSoapWrapper soapWrapper(
        m_onvifNotificationSubscriptionReference.isEmpty()
            ? m_eventCapabilities->XAddr
            : m_onvifNotificationSubscriptionReference.toLatin1().constData(),
        auth.user(),
        auth.password(),
        m_timeDrift );
    soapWrapper.getProxy()->soap->imode |= SOAP_XML_IGNORENS;

    char buf[256];

    _oasisWsnB2__Unsubscribe request;
    if( !m_onvifNotificationSubscriptionID.isEmpty() )
    {
        sprintf( buf, "<dom0:SubscriptionId xmlns:dom0=\"http://www.onvifplus.org/event\">%s</dom0:SubscriptionId>", m_onvifNotificationSubscriptionID.toLatin1().data() );
        request.__any.push_back( buf );
    }
    _oasisWsnB2__UnsubscribeResponse response;
    const int soapCallResult = soapWrapper.unsubscribe( request, response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOGX( lit("Failed to unsubscuibe from %1, result code %2").
            arg(QString::fromLatin1(soapWrapper.endpoint())).arg(soapCallResult), cl_logDEBUG1 );
        return;
    }
}

bool QnPlOnvifResource::isInputPortMonitored() const
{
    QnMutexLocker lk( &m_ioPortMutex );
    return m_inputMonitored;
}

template<class _NumericInt>
_NumericInt roundUp( _NumericInt val, _NumericInt step, typename std::enable_if<std::is_integral<_NumericInt>::value>::type* = nullptr )
{
    if( step == 0 )
        return val;
    return (val + step - 1) / step * step;
}

void QnPlOnvifResource::pullMessages(quint64 timerID)
{
    static const int MAX_MESSAGES_TO_PULL = 10;

    QnMutexLocker lk( &m_ioPortMutex );

    if( timerID != m_nextPullMessagesTimerID )
        return; //not expected event. This can actually happen if we call
                //startInputPortMonitoring, stopInputPortMonitoring, startInputPortMonitoring really quick
    m_nextPullMessagesTimerID = 0;

    if( !m_inputMonitored )
        return;

    if( m_asyncPullMessagesCallWrapper )
        return; //previous request is still running, new timer will be added within completion handler

    QAuthenticator auth = getAuth();

    std::unique_ptr<PullPointSubscriptionWrapper> soapWrapper(
        new PullPointSubscriptionWrapper(
            m_onvifNotificationSubscriptionReference.isEmpty()
                ? m_eventCapabilities->XAddr
                : m_onvifNotificationSubscriptionReference.toStdString(),
            auth.user(),
            auth.password(),
            m_timeDrift ) );
    soapWrapper->getProxy()->soap->imode |= SOAP_XML_IGNORENS;

    std::vector<void*> memToFreeOnResponseDone;
    memToFreeOnResponseDone.reserve(3); //we have 3 memory allocation below

    char* buf = (char*)malloc(512);
    memToFreeOnResponseDone.push_back(buf);

    _onvifEvents__PullMessages request;
    sprintf( buf, "PT%lldS", roundUp<qint64>(m_monotonicClock.elapsed() - m_prevPullMessageResponseClock, MS_PER_SECOND) / MS_PER_SECOND );
    request.Timeout = buf;
    request.MessageLimit = MAX_MESSAGES_TO_PULL;
    QByteArray onvifNotificationSubscriptionIDLatin1 = m_onvifNotificationSubscriptionID.toLatin1();
    strcpy(buf, onvifNotificationSubscriptionIDLatin1.data());
    struct SOAP_ENV__Header* header = (struct SOAP_ENV__Header*)malloc(sizeof(SOAP_ENV__Header));
    memToFreeOnResponseDone.push_back(header);
    memset( header, 0, sizeof(*header) );
    soapWrapper->getProxy()->soap->header = header;
    soapWrapper->getProxy()->soap->header->subscriptionID = buf;
    //TODO #ak move away check for "Samsung"
    if( !m_onvifNotificationSubscriptionReference.isEmpty() && !getVendor().contains(lit("Samsung")) )
    {
        const QByteArray& onvifNotificationSubscriptionReferenceUtf8 = m_onvifNotificationSubscriptionReference.toUtf8();
        char* buf = (char*)malloc(onvifNotificationSubscriptionReferenceUtf8.size()+1);
        memToFreeOnResponseDone.push_back(buf);
        strcpy( buf, onvifNotificationSubscriptionReferenceUtf8.constData() );
        soapWrapper->getProxy()->soap->header->wsa__To = buf;
    }
    _onvifEvents__PullMessagesResponse response;

    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    const bool useHttpReader = resData.value<bool>(
        Qn::PARSE_ONVIF_NOTIFICATIONS_WITH_HTTP_READER,
        false);

    QSharedPointer<GSoapAsyncPullMessagesCallWrapper> asyncPullMessagesCallWrapper(
        new GSoapAsyncPullMessagesCallWrapper(
            std::move(soapWrapper),
            &PullPointSubscriptionWrapper::pullMessages,
            useHttpReader),
        [memToFreeOnResponseDone](GSoapAsyncPullMessagesCallWrapper* ptr){
            for( void* pObj: memToFreeOnResponseDone )
                ::free( pObj );
            delete ptr;
        }
    );

    NX_VERBOSE(this, lm("Pull messages from %1 with httpReader=%2").args(
        m_onvifNotificationSubscriptionReference, useHttpReader));

    using namespace std::placeholders;
    asyncPullMessagesCallWrapper->callAsync(
        request,
        std::bind(&QnPlOnvifResource::onPullMessagesDone, this, asyncPullMessagesCallWrapper.data(), _1));
    m_asyncPullMessagesCallWrapper = std::move(asyncPullMessagesCallWrapper);
}

void QnPlOnvifResource::onPullMessagesDone(GSoapAsyncPullMessagesCallWrapper* asyncWrapper, int resultCode)
{
    using namespace std::placeholders;

    auto SCOPED_GUARD_FUNC = [this]( QnPlOnvifResource* ){
        m_asyncPullMessagesCallWrapper.clear();
    };
    std::unique_ptr<QnPlOnvifResource, decltype(SCOPED_GUARD_FUNC)>
        SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

    if( (resultCode != SOAP_OK && resultCode != SOAP_MUSTUNDERSTAND) ||  //error has been reported by camera
        (asyncWrapper->response().soap &&
            asyncWrapper->response().soap->header &&
            asyncWrapper->response().soap->header->wsa__Action &&
            strstr(asyncWrapper->response().soap->header->wsa__Action, "/soap/fault") != nullptr))
    {
        NX_LOGX( lit("Failed to pull messages from %1, result code %2").
            arg(QString::fromLatin1(asyncWrapper->syncWrapper()->endpoint())).
            arg(resultCode), cl_logDEBUG1 );
        //re-subscribing

        QnMutexLocker lk( &m_ioPortMutex );

        if( !m_inputMonitored )
            return;

        return updateTimer( &m_renewSubscriptionTimerID, std::chrono::milliseconds::zero(),
            std::bind(&QnPlOnvifResource::renewPullPointSubscriptionFallback, this, _1) );
    }

    onPullMessagesResponseReceived(asyncWrapper->syncWrapper(), resultCode, asyncWrapper->response());

    QnMutexLocker lk( &m_ioPortMutex );

    if( !m_inputMonitored )
        return;

    using namespace std::placeholders;
    NX_ASSERT( m_nextPullMessagesTimerID == 0 );
    if( m_nextPullMessagesTimerID == 0 )    //otherwise, we already have timer somehow
        m_nextPullMessagesTimerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnPlOnvifResource::pullMessages, this, _1),
            std::chrono::milliseconds(PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND));
}

void QnPlOnvifResource::renewPullPointSubscriptionFallback(quint64 timerId)
{
    QnMutexLocker lock(&m_ioPortMutex);
    if (timerId != m_renewSubscriptionTimerID)
        return;
    if (!m_inputMonitored)
        return;

    bool isSubscribed = false;
    {
        QnMutexUnlocker unlock(&lock);
        // TODO: Make removePullPointSubscription and createPullPointSubscription
        // asynchronous, so that it does not block timer thread.
        removePullPointSubscription();
        isSubscribed = createPullPointSubscription();
    }

    if (isSubscribed)
        m_renewSubscriptionTimerID = 0;
    else
        scheduleRetrySubscriptionTimer();
}

void QnPlOnvifResource::onPullMessagesResponseReceived(
    PullPointSubscriptionWrapper* /*soapWrapper*/,
    int resultCode,
    const _onvifEvents__PullMessagesResponse& response)
{
    NX_ASSERT( resultCode == SOAP_OK || resultCode == SOAP_MUSTUNDERSTAND );

    const qint64 currentRequestSendClock = m_monotonicClock.elapsed();

    const time_t minNotificationTime = response.CurrentTime - roundUp<qint64>(m_monotonicClock.elapsed() - m_prevPullMessageResponseClock, MS_PER_SECOND) / MS_PER_SECOND;
    if( response.oasisWsnB2__NotificationMessage.size() > 0 )
    {
        for( size_t i = 0;
            i < response.oasisWsnB2__NotificationMessage.size();
            ++i )
        {
            notificationReceived(*response.oasisWsnB2__NotificationMessage[i], minNotificationTime);
        }
    }

    m_prevPullMessageResponseClock = currentRequestSendClock;
}

bool QnPlOnvifResource::fetchRelayOutputs( std::vector<RelayOutputInfo>* const relayOutputs )
{
    QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift );

    _onvifDevice__GetRelayOutputs request;
    _onvifDevice__GetRelayOutputsResponse response;
    const int soapCallResult = soapWrapper.getRelayOutputs( request, response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOGX( lit("Failed to get relay input/output info. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
        return false;
    }

    m_relayOutputInfo.clear();
    if (response.RelayOutputs.size() > MAX_IO_PORTS_PER_DEVICE)
    {
        NX_LOGX( lit("Device has too many relay outputs. endpoint %1")
            .arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
        return false;
    }

    for( size_t i = 0; i < response.RelayOutputs.size(); ++i )
    {
        m_relayOutputInfo.push_back( RelayOutputInfo(
            response.RelayOutputs[i]->token,
            response.RelayOutputs[i]->Properties->Mode == onvifXsd__RelayMode__Bistable,
            response.RelayOutputs[i]->Properties->DelayTime,
            response.RelayOutputs[i]->Properties->IdleState == onvifXsd__RelayIdleState__closed ) );
    }

    if( relayOutputs )
        *relayOutputs = m_relayOutputInfo;

    NX_LOGX( lit("Successfully got device (%1) output ports info. Found %2 relay output").
        arg(QString::fromLatin1(soapWrapper.endpoint())).arg(m_relayOutputInfo.size()), cl_logDEBUG1 );

    return true;
}

bool QnPlOnvifResource::fetchRelayOutputInfo( const std::string& outputID, RelayOutputInfo* const relayOutputInfo )
{
    fetchRelayOutputs( NULL );
    for( std::vector<RelayOutputInfo>::size_type
         i = 0;
         i < m_relayOutputInfo.size();
        ++i )
    {
        if( m_relayOutputInfo[i].token == outputID || outputID.empty() )
        {
            *relayOutputInfo = m_relayOutputInfo[i];
            return true;
        }
    }

    return false; //there is no output with id outputID
}

bool QnPlOnvifResource::setRelayOutputSettings( const RelayOutputInfo& relayOutputInfo )
{
    QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift );

    NX_LOGX( lit("Swiching camera %1 relay output %2 to monostable mode").
        arg(QString::fromLatin1(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)), cl_logDEBUG1 );

    //switching to monostable mode
    _onvifDevice__SetRelayOutputSettings setOutputSettingsRequest;
    setOutputSettingsRequest.RelayOutputToken = relayOutputInfo.token;
    onvifXsd__RelayOutputSettings relayOutputSettings;
    relayOutputSettings.Mode = relayOutputInfo.isBistable ? onvifXsd__RelayMode__Bistable : onvifXsd__RelayMode__Monostable;
    relayOutputSettings.DelayTime = !relayOutputInfo.delayTime.empty() ? relayOutputInfo.delayTime : "PT1S";
    relayOutputSettings.IdleState = relayOutputInfo.activeByDefault ? onvifXsd__RelayIdleState__closed : onvifXsd__RelayIdleState__open;
    setOutputSettingsRequest.Properties = &relayOutputSettings;
    _onvifDevice__SetRelayOutputSettingsResponse setOutputSettingsResponse;
    const int soapCallResult = soapWrapper.setRelayOutputSettings( setOutputSettingsRequest, setOutputSettingsResponse );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOGX( lit("Failed to switch camera %1 relay output %2 to monostable mode. %3").
            arg(QString::fromLatin1(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)).arg(soapCallResult), cl_logWARNING );
        return false;
    }

    return true;
}

int QnPlOnvifResource::getMaxChannels() const
{
    return m_maxChannels;
}

void QnPlOnvifResource::updateToChannel(int value)
{
    QString suffix = QString(QLatin1String("?channel=%1")).arg(value+1);
    setUrl(getUrl() + suffix);
    setPhysicalId(getPhysicalId() + suffix.replace(QLatin1String("?"), QLatin1String("_")));
    setName(getName() + QString(QLatin1String("-channel %1")).arg(value+1));
}

QnConstResourceVideoLayoutPtr QnPlOnvifResource::getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider) const
{
    if (m_videoLayout)
        return m_videoLayout;

    auto resData = qnStaticCommon->dataPool()->data(getVendor(), getModel());
    auto layoutStr = resData.value<QString>(Qn::VIDEO_LAYOUT_PARAM_NAME2);

    if (!layoutStr.isEmpty())
    {
        m_videoLayout = QnResourceVideoLayoutPtr(
            QnCustomResourceVideoLayout::fromString(layoutStr));
    }
    else
    {
        m_videoLayout = QnMediaResource::getVideoLayout(dataProvider)
            .constCast<QnResourceVideoLayout>();
    }

    auto resourceId = getId();

    auto nonConstThis = const_cast<QnPlOnvifResource*>(this);
    nonConstThis->setProperty(Qn::VIDEO_LAYOUT_PARAM_NAME, m_videoLayout->toString());
    nonConstThis->saveParams();

    return m_videoLayout;
}

bool QnPlOnvifResource::secondaryResolutionIsLarge() const
{
    return m_secondaryResolution.width() * m_secondaryResolution.height() > 720 * 480;
}

void QnPlOnvifResource::setRelayOutputStateNonSafe(
    quint64 timerID,
    const QString& outputID,
    bool active,
    unsigned int autoResetTimeoutMS )
{
    {
        QnMutexLocker lk( &m_ioPortMutex );
        m_triggerOutputTasks.erase( timerID );
    }

    //retrieving output info to check mode
    RelayOutputInfo relayOutputInfo;
    if( !fetchRelayOutputInfo( outputID.toStdString(), &relayOutputInfo ) )
    {
        NX_LOGX( lit("Cannot change relay output %1 state. Failed to get relay output info").arg(outputID), cl_logWARNING );
        return /*false*/;
    }

#ifdef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    const bool isBistableModeRequired = true;
#else
    const bool isBistableModeRequired = autoResetTimeoutMS == 0;
#endif

#ifndef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    std::string requiredDelayTime;
    if( autoResetTimeoutMS > 0 )
    {
        std::ostringstream ss;
        ss<<"PT"<<(autoResetTimeoutMS < 1000 ? 1 : autoResetTimeoutMS/1000)<<"S";
        requiredDelayTime = ss.str();
    }
#endif
    if( (relayOutputInfo.isBistable != isBistableModeRequired) ||
#ifndef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
        (!isBistableModeRequired && relayOutputInfo.delayTime != requiredDelayTime) ||
#endif
        relayOutputInfo.activeByDefault )
    {
        //switching output to required mode
        relayOutputInfo.isBistable = isBistableModeRequired;
#ifndef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
        relayOutputInfo.delayTime = requiredDelayTime;
#endif
        relayOutputInfo.activeByDefault = false;
        if( !setRelayOutputSettings( relayOutputInfo ) )
        {
            NX_LOGX( lit("Cannot set camera %1 output %2 to state %3 with timeout %4 msec. Cannot set mode to %5").
                arg(QString()).arg(QString::fromStdString(relayOutputInfo.token)).arg(QLatin1String(active ? "active" : "inactive")).arg(autoResetTimeoutMS).
                arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")), cl_logWARNING );
            return /*false*/;
        }

        NX_LOGX( lit("Camera %1 output %2 has been switched to %3 mode").arg(QString()).arg(outputID).
            arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")), cl_logWARNING );
    }

    //modifying output
    QAuthenticator auth = getAuth();

    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift );

    _onvifDevice__SetRelayOutputState request;
    request.RelayOutputToken = relayOutputInfo.token;

    const auto onvifActive = m_isRelayOutputInversed ? !active : active;
    request.LogicalState = onvifActive ? onvifXsd__RelayLogicalState__active : onvifXsd__RelayLogicalState__inactive;

    _onvifDevice__SetRelayOutputStateResponse response;
    const int soapCallResult = soapWrapper.setRelayOutputState( request, response );
    if( soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOGX( lm("Failed to set relay %1 output state to %2. endpoint %3")
            .args(relayOutputInfo.token, onvifActive, soapWrapper.endpoint()), cl_logWARNING );
        return /*false*/;
    }

#ifdef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    if( (autoResetTimeoutMS > 0) && active )
    {
        //adding task to reset port state
        using namespace std::placeholders;
        const quint64 timerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnPlOnvifResource::setRelayOutputStateNonSafe, this, _1, outputID, !active, 0),
            std::chrono::milliseconds(autoResetTimeoutMS));
        m_triggerOutputTasks[timerID] = TriggerOutputTask(outputID, !active, 0);
    }
#endif

    NX_LOGX( lm("Successfully set relay %1 output state to %2. endpoint %3")
        .args(relayOutputInfo.token, onvifActive, soapWrapper.endpoint()), cl_logINFO );
    return /*true*/;
}

QnMutex* QnPlOnvifResource::getStreamConfMutex()
{
    return &m_streamConfMutex;
}

void QnPlOnvifResource::beforeConfigureStream(Qn::ConnectionRole /*role*/)
{
    QnMutexLocker lock( &m_streamConfMutex );
    ++m_streamConfCounter;
    while (m_streamConfCounter > 1)
        m_streamConfCond.wait(&m_streamConfMutex);
}

void QnPlOnvifResource::afterConfigureStream(Qn::ConnectionRole /*role*/)
{
    QnMutexLocker lock( &m_streamConfMutex );
    --m_streamConfCounter;
    m_streamConfCond.wakeAll();
    while (m_streamConfCounter > 0)
        m_streamConfCond.wait(&m_streamConfMutex);
}

CameraDiagnostics::Result QnPlOnvifResource::customStreamConfiguration(Qn::ConnectionRole role)
{
    return CameraDiagnostics::NoErrorResult();
}

double QnPlOnvifResource::getClosestAvailableFps(double desiredFps)
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool useEncodingInterval = resData.value<bool>(
        Qn::CONTROL_FPS_VIA_ENCODING_INTERVAL_PARAM_NAME);

    if (useEncodingInterval)
    {
        int fpsBase = resData.value<int>(Qn::FPS_BASE_PARAM_NAME);
        int encodingInterval = 1;
        double bestDiff = fpsBase;
        double bestFps = 1;

        if (!fpsBase)
            return desiredFps;

        while (fpsBase > encodingInterval)
        {
            auto fpsCandidate = fpsBase / encodingInterval;
            auto currentDiff = desiredFps - fpsCandidate;
            if (currentDiff < bestDiff)
            {
                bestDiff = currentDiff;
                bestFps = fpsCandidate;
            }
            else
            {
                break;
            }

            encodingInterval++;
        }

        return bestFps;
    }
    else
    {
        return desiredFps;
    }
}

void QnPlOnvifResource::updateFirmware()
{
    QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    DeviceInfoReq request;
    DeviceInfoResp response;
    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes == SOAP_OK)
    {
        QString firmware = QString::fromStdString(response.FirmwareVersion);
        if (!firmware.isEmpty())
            setFirmware(firmware);
    }
}

CameraDiagnostics::Result QnPlOnvifResource::getFullUrlInfo()
{
    QAuthenticator auth = getAuth();

    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);
    CapabilitiesResp response;
    auto result = fetchOnvifCapabilities( &soapWrapper, &response );
    if( !result )
        return result;

    if (response.Capabilities)
        fillFullUrlInfo( response );

    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::updateOnvifUrls(const QnPlOnvifResourcePtr& other)
{
    setDeviceOnvifUrl(other->getDeviceOnvifUrl());
    setMediaUrl(other->getMediaUrl());
    setImagingUrl(other->getImagingUrl());
    setPtzUrl(other->getPtzUrl());
}

CameraDiagnostics::Result QnPlOnvifResource::fetchOnvifCapabilities(
    DeviceSoapWrapper* const soapWrapper,
    CapabilitiesResp* const response)
{
    QAuthenticator auth = getAuth();

    //Trying to get onvif URLs
    CapabilitiesReq request;

    int soapRes = soapWrapper->getCapabilities(request, *response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
            << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
#endif
        if (soapWrapper->isNotAuthenticated())
        {
            if (!getId().isNull())
                setStatus(Qn::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult( getDeviceOnvifUrl() );
        }
        return CameraDiagnostics::RequestFailedResult(lit("getCapabilities"), soapWrapper->getLastError());
    }

    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::fillFullUrlInfo( const CapabilitiesResp& response )
{
    if (!response.Capabilities)
        return;

    if (response.Capabilities->Events)
    {
        m_eventCapabilities.reset(new onvifXsd__EventCapabilities(*response.Capabilities->Events));
        m_eventCapabilities->XAddr = fromOnvifDiscoveredUrl(m_eventCapabilities->XAddr).toStdString();
    }

    if (response.Capabilities->Media)
    {
        setMediaUrl(fromOnvifDiscoveredUrl(response.Capabilities->Media->XAddr));
    }
    if (response.Capabilities->Imaging)
    {
        setImagingUrl(fromOnvifDiscoveredUrl(response.Capabilities->Imaging->XAddr));
    }
    if (response.Capabilities->Device)
    {
        setDeviceOnvifUrl(fromOnvifDiscoveredUrl(response.Capabilities->Device->XAddr));
    }
    if (response.Capabilities->PTZ)
    {
        setPtzUrl(fromOnvifDiscoveredUrl(response.Capabilities->PTZ->XAddr));
    }
    if (response.Capabilities->Extension && response.Capabilities->Extension->DeviceIO)
    {
        m_deviceIOUrl = fromOnvifDiscoveredUrl(response.Capabilities->Extension->DeviceIO->XAddr)
            .toStdString();
    }
    else
    {
        m_deviceIOUrl = getDeviceOnvifUrl().toStdString();
    }
}

/**
 * Some cameras provide model in a bit different way via native driver and ONVIF driver.
 * Try several variants to match json data.
 */
bool QnPlOnvifResource::isCameraForcedToOnvif(const QString& manufacturer, const QString& model)
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacturer, model);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return true;

    QString shortModel = model;
    shortModel.replace(QString(lit(" ")), QString());
    shortModel.replace(QString(lit("-")), QString());
    resourceData = qnStaticCommon->dataPool()->data(manufacturer, shortModel);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return true;

    if (shortModel.startsWith(manufacturer))
        shortModel = shortModel.mid(manufacturer.length()).trimmed();
    resourceData = qnStaticCommon->dataPool()->data(manufacturer, shortModel);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return true;

    return false;
}

bool QnPlOnvifResource::initializeTwoWayAudio()
{
    // TODO: move this function to the PhysicalCamResource class
    const QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    TwoWayAudioParams params = resourceData.value<TwoWayAudioParams>(Qn::TWO_WAY_AUDIO_PARAM_NAME);
    if (params.codec.isEmpty() || params.urlPath.isEmpty())
        return false;

    QnAudioFormat format;
    format.setCodec(params.codec);
    format.setSampleRate(params.sampleRate * 1000);
    format.setChannelCount(params.channels);
    auto audioTransmitter = new QnBasicAudioTransmitter(this);
    m_audioTransmitter.reset(audioTransmitter);
    m_audioTransmitter->setOutputFormat(format);
    m_audioTransmitter->setBitrateKbps(params.bitrateKbps * 1000);
    audioTransmitter->setContentType(params.contentType.toUtf8());
    if (params.noAuth)
        audioTransmitter->setAuthPolicy(QnBasicAudioTransmitter::AuthPolicy::noAuth);
    else if (params.useBasicAuth)
        audioTransmitter->setAuthPolicy(QnBasicAudioTransmitter::AuthPolicy::basicAuth);
    else
        audioTransmitter->setAuthPolicy(QnBasicAudioTransmitter::AuthPolicy::digestAndBasicAuth);

    QUrl srcUrl(getUrl());
    QUrl url(lit("http://%1:%2%3").arg(srcUrl.host()).arg(srcUrl.port()).arg(params.urlPath));
    audioTransmitter->setTransmissionUrl(url);

    return true;
}

#endif //ENABLE_ONVIF
