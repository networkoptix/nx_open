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
#include <onvif/soapMedia2BindingProxy.h>
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
#include <nx/network/aio/aio_service.h>
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
#include <plugins/resource/onvif/onvif_audio_transmitter.h>
#include <plugins/resource/onvif/onvif_maintenance_proxy.h>

#include <nx/fusion/model_functions.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <core/dataconsumer/basic_audio_transmitter.h>

#include <plugins/utils/multisensor_data_provider.h>
#include <core/resource_management/resource_properties.h>
#include <utils/media/av_codec_helper.h>

//!assumes that camera can only work in bistable mode (true for some (or all?) DW cameras)
#define SIMULATE_RELAY_PORT_MOMOSTABLE_MODE

namespace
{
    const std::string kOnvifMedia2Namespace("http://www.onvif.org/ver20/media/wsdl");

    const QString kBaselineH264Profile("Baseline");
    const QString kMainH264Profile("Main");
    const QString kExtendedH264Profile("Extended");
    const QString kHighH264Profile("High");

    onvifXsd__H264Profile fromStringToH264Profile(const QString& str)
    {
        if (str == kMainH264Profile)
            return onvifXsd__H264Profile::Main;
        else if (str == kExtendedH264Profile)
            return onvifXsd__H264Profile::Extended;
        else if (str == kHighH264Profile)
            return onvifXsd__H264Profile::High;
        else
            return onvifXsd__H264Profile::Baseline;
    };
}

const QString QnPlOnvifResource::MANUFACTURE(lit("OnvifDevice"));
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
// !If renew subscription exactly at termination time, camera can already terminate subscription,
// so have to do that a little bit earlier..
static const unsigned int RENEW_NOTIFICATION_FORWARDING_SECS = 5;
static const unsigned int MS_PER_SECOND = 1000;
static const unsigned int PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC = 1;
static const int MAX_IO_PORTS_PER_DEVICE = 200;
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

QString QnOnvifServiceUrls::getUrl(OnvifWebService onvifWebService) const
{
    switch (onvifWebService)
    {
        case OnvifWebService::Media: return mediaServiceUrl;
        case OnvifWebService::Media2: return media2ServiceUrl;
        case OnvifWebService::Ptz: return ptzServiceUrl;
        case OnvifWebService::Imaging: return imagingServiceUrl;
        case OnvifWebService::DeviceIO: return deviceioServiceUrl;

    }
    NX_ASSERT(0, QString("Unknown value of OnvifWebService enum: %1")
        .arg(static_cast<int>(onvifWebService)));
    return QString();
}

QnPlOnvifResource::VideoOptionsLocal::VideoOptionsLocal(
    const QString& id,
    const onvifXsd__VideoEncoderConfigurationOptions& options,
    QnBounds frameRateBounds)
    :
    id(id)
{
    std::vector<onvifXsd__VideoResolution*>* srcVector = 0;
    if (options.H264)
        srcVector = &options.H264->ResolutionsAvailable;
    else if (options.JPEG)
        srcVector = &options.JPEG->ResolutionsAvailable;
    if (srcVector)
    {
        for (uint i = 0; i < srcVector->size(); ++i)
            resolutions << QSize(srcVector->at(i)->Width, srcVector->at(i)->Height);
    }

    if (options.H264)
    {
        encoding = UnderstandableVideoCodec::H264;

        for (uint i = 0; i < options.H264->H264ProfilesSupported.size(); ++i)
            h264Profiles << options.H264->H264ProfilesSupported[i];
        std::sort(h264Profiles.begin(), h264Profiles.end());

        if (options.H264->FrameRateRange)
        {
            frameRateMax = restrictFrameRate(
                options.H264->FrameRateRange->Max, frameRateBounds);
            frameRateMin = restrictFrameRate(
                options.H264->FrameRateRange->Min, frameRateBounds);
        }

        if (options.H264->GovLengthRange)
        {
            govMin = options.H264->GovLengthRange->Min;
            govMax = options.H264->GovLengthRange->Max;
        }
    }
    else if (options.JPEG)
    {
        encoding = UnderstandableVideoCodec::JPEG;

        if (options.JPEG->FrameRateRange)
        {
            frameRateMax = restrictFrameRate(
                options.JPEG->FrameRateRange->Max, frameRateBounds);
            frameRateMin = restrictFrameRate(
                options.JPEG->FrameRateRange->Min, frameRateBounds);
        }
    }
    if (options.QualityRange)
    {
        minQ = options.QualityRange->Min;
        maxQ = options.QualityRange->Max;
    }
}

QnPlOnvifResource::VideoOptionsLocal::VideoOptionsLocal(const QString& id,
    const onvifXsd__VideoEncoder2ConfigurationOptions& resp,
    QnBounds frameRateBounds)
    :
    id(id)
{
    VideoEncoderConfigOptions options(resp);

    for (QSize resolution: options.resolutions)
        this->resolutions << resolution;

    this->encoding = options.encoder;

    if (encoding == UnderstandableVideoCodec::H264)
    {
        for (auto profile: options.encoderProfiles)
        {
            switch (profile)
            {

            case onvifXsd__VideoEncodingProfiles::Baseline:
                h264Profiles.push_back(onvifXsd__H264Profile::Baseline);
                break;
            case onvifXsd__VideoEncodingProfiles::Main:
                h264Profiles.push_back(onvifXsd__H264Profile::Main);
                break;
            case onvifXsd__VideoEncodingProfiles::Extended:
                h264Profiles.push_back(onvifXsd__H264Profile::Extended);
                break;
            case onvifXsd__VideoEncodingProfiles::High:
                h264Profiles.push_back(onvifXsd__H264Profile::High);
                break;
            }

        }
        std::sort(h264Profiles.begin(), h264Profiles.end());
    }
    if (encoding == UnderstandableVideoCodec::H265)
    {
        for (const auto profile: options.encoderProfiles)
            h265Profiles.push_back(profile);
        std::sort(h265Profiles.begin(), h265Profiles.end());
    }

    if (!options.frameRates.empty())
    {
        frameRateMax = restrictFrameRate(
            options.frameRates.back(), frameRateBounds);
        frameRateMin = restrictFrameRate(
            options.frameRates.front(), frameRateBounds);
    }

    if (options.govLengthRange)
    {
        govMin = options.govLengthRange->low;
        govMax = options.govLengthRange->high;
    }

    /* They forgot to add round to std in arm! So we need a hack here.*/
    using namespace std;
    minQ = round(options.qualityRange.low);
    maxQ = round(options.qualityRange.high);
}

int QnPlOnvifResource::VideoOptionsLocal::restrictFrameRate(
    int frameRate, QnBounds frameRateBounds) const
{
    if (frameRateBounds.isNull())
        return frameRate;

    return qBound((int)frameRateBounds.min, frameRate, (int)frameRateBounds.max);
}

typedef std::function<bool(
    const QnPlOnvifResource::VideoOptionsLocal&,
    const QnPlOnvifResource::VideoOptionsLocal&)> VideoOptionsComparator;

bool videoOptsGreaterThan(
    const QnPlOnvifResource::VideoOptionsLocal &s1,
    const QnPlOnvifResource::VideoOptionsLocal &s2)
{
    int square1Max = 0;
    QSize max1Res;
    for (int i = 0; i < s1.resolutions.size(); ++i)
    {
        int newMax = s1.resolutions[i].width() * s1.resolutions[i].height();
        if (newMax > square1Max)
        {
            square1Max = newMax;
            max1Res = s1.resolutions[i];
        }
    }

    int square2Max = 0;
    QSize max2Res;
    for (int i = 0; i < s2.resolutions.size(); ++i)
    {
        int newMax = s2.resolutions[i].width() * s2.resolutions[i].height();
        if (newMax > square2Max)
        {
            square2Max = newMax;
            max2Res = s2.resolutions[i];
        }
    }

    if (square1Max != square2Max)
        return square1Max > square2Max;

    //for equal resolutions the rule is: H264 > H265 > JPEG
    if (s1.encoding != s2.encoding)
        return s1.encoding > s2.encoding;

    if (!s1.usedInProfiles && s2.usedInProfiles)
        return false;
    else if (s1.usedInProfiles && !s2.usedInProfiles)
        return true;

    return s1.id > s2.id; // sort by name
}

bool compareByProfiles(
    const QnPlOnvifResource::VideoOptionsLocal &s1,
    const QnPlOnvifResource::VideoOptionsLocal &s2,
    const QMap<QString, int>& profilePriorities)
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
            [profilePriorities](
                const QnPlOnvifResource::VideoOptionsLocal &s1,
                const QnPlOnvifResource::VideoOptionsLocal &s2) -> bool
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

QnPlOnvifResource::QnPlOnvifResource(QnMediaServerModule* serverModule):
    base_type(serverModule),
    m_audioCodec(AUDIO_NONE),
    m_audioBitrate(0),
    m_audioSamplerate(0),
    m_timeDrift(0),
    m_isRelayOutputInversed(false),
    m_fixWrongInputPortNumber(false),
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
    m_advancedParametersProvider(this),
    m_onvifRecieveTimeout(DEFAULT_SOAP_TIMEOUT),
    m_onvifSendTimeout(DEFAULT_SOAP_TIMEOUT)
{
    m_tmpH264Conf.reset(new onvifXsd__H264Configuration());
    m_monotonicClock.start();
    m_advSettingsLastUpdated.restart();
}

QnPlOnvifResource::~QnPlOnvifResource()
{
    {
        QnMutexLocker lk(&m_ioPortMutex);
        while(!m_triggerOutputTasks.empty())
        {
            const quint64 timerID = m_triggerOutputTasks.begin()->first;
            const TriggerOutputTask outputTask = m_triggerOutputTasks.begin()->second;
            m_triggerOutputTasks.erase(m_triggerOutputTasks.begin());

            lk.unlock();

            // Garantees that no onTimer(timerID) is running on return.
            nx::utils::TimerManager::instance()->joinAndDeleteTimer(timerID);
            if (!outputTask.active)
            {
                //returning port to inactive state
                setOutputPortStateNonSafe(
                    0,
                    outputTask.outputID,
                    outputTask.active,
                    0);
            }

            lk.relock();
        }
    }

    stopInputPortStatesMonitoring();

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

            if (conf->DHCP && conf->FromDHCP)
            {
                // TODO: #vasilenko UTF unuse std::string
                if (senderIpAddress == QString::fromStdString(conf->FromDHCP->Address))
                {
                    return QString::fromStdString(ifacePtr->Info->HwAddress).toUpper()
                        .replace(QLatin1Char(':'), QLatin1Char('-'));
                }
                if (someMacAddress.isEmpty())
                {
                    someMacAddress = QString::fromStdString(ifacePtr->Info->HwAddress);
                }
            }

            std::vector<class onvifXsd__PrefixedIPv4Address*> addresses = conf->Manual;
            std::vector<class onvifXsd__PrefixedIPv4Address*>::const_iterator addrPtrIter =
                addresses.begin();

            for (; addrPtrIter != addresses.end(); ++addrPtrIter)
            {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;
                if (!addrPtr)
                    continue;

                // TODO: #vasilenko UTF unuse std::string
                if (senderIpAddress == QString::fromStdString(addrPtr->Address))
                {
                    return QString::fromStdString(ifacePtr->Info->HwAddress).toUpper()
                        .replace(QLatin1Char(':'), QLatin1Char('-'));
                }
                if (someMacAddress.isEmpty())
                {
                    someMacAddress = QString::fromStdString(ifacePtr->Info->HwAddress);
                }
            }
        }
    }

    return someMacAddress.toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
}

void QnPlOnvifResource::setHostAddress(const QString &ip)
{
    //nx::mediaserver::resource::Camera::se
    {
        QnMutexLocker lock(&m_mutex);

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

    nx::mediaserver::resource::Camera::setHostAddress(ip);
}

const QString QnPlOnvifResource::createOnvifEndpointUrl(const QString& ipAddress)
{
    return QLatin1String(ONVIF_PROTOCOL_PREFIX) + ipAddress + QLatin1String(ONVIF_URL_SUFFIX);
}

typedef GSoapAsyncCallWrapper <
    DeviceSoapWrapper,
    NetIfacesReq,
    NetIfacesResp
> GSoapDeviceGetNetworkIntfAsyncWrapper;

void QnPlOnvifResource::checkIfOnlineAsync(std::function<void(bool)> completionHandler)
{
    QAuthenticator auth = getAuth();

    const QString deviceUrl = getDeviceOnvifUrl();
    if (deviceUrl.isEmpty())
    {
        //calling completionHandler(false)
        nx::network::SocketGlobals::aioService().post(std::bind(completionHandler, false));
        return;
    }

    std::unique_ptr<DeviceSoapWrapper> soapWrapper(new DeviceSoapWrapper(
        onvifTimeouts(),
        deviceUrl.toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift));

    // Trying to get HardwareId.
    auto asyncWrapper = std::make_shared<GSoapDeviceGetNetworkIntfAsyncWrapper>(
        std::move(soapWrapper),
        &DeviceSoapWrapper::getNetworkInterfaces );

    const nx::utils::MacAddress resourceMAC = getMAC();
    auto onvifCallCompletionFunc =
        [asyncWrapper, deviceUrl, resourceMAC, completionHandler]( int soapResultCode )
        {
            if (soapResultCode != SOAP_OK)
                return completionHandler( false );

            completionHandler(
                resourceMAC.toString() == QnPlOnvifResource::fetchMacAddress(
                    asyncWrapper->response(), QUrl(deviceUrl).host()));
        };

    NetIfacesReq request;
    asyncWrapper->callAsync(request, onvifCallCompletionFunc);
}

QString QnPlOnvifResource::getDriverName() const
{
    return MANUFACTURE;
}

const QSize QnPlOnvifResource::getVideoSourceSize() const
{
    QnMutexLocker lock(&m_mutex);
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

QnPlOnvifResource::AUDIO_CODEC QnPlOnvifResource::getAudioCodec() const
{
    QnMutexLocker lock(&m_mutex);
    return m_audioCodec;
}

void QnPlOnvifResource::setAudioCodec(QnPlOnvifResource::AUDIO_CODEC c)
{
    QnMutexLocker lock(&m_mutex);
    m_audioCodec = c;
}

QnAbstractStreamDataProvider* QnPlOnvifResource::createLiveDataProvider()
{
    auto resData = resourceData();
    bool shouldAppearAsSingleChannel = resData.value<bool>(
        Qn::SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME);

    if (shouldAppearAsSingleChannel)
        return new nx::plugins::utils::MultisensorDataProvider(toSharedPointer(this));

    return new QnOnvifStreamReader(toSharedPointer(this));
}

nx::mediaserver::resource::StreamCapabilityMap QnPlOnvifResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex streamIndex)
{
#if 1
    //old version
    using namespace nx::mediaserver::resource;

    QnMutexLocker lock(&m_mutex);

    const auto& capabilities = streamIndex == Qn::StreamIndex::primary
        ? m_primaryStreamCapabilities: m_secondaryStreamCapabilities;

    StreamCapabilityKey key;
    switch (capabilities.encoding)
    {
        case UnderstandableVideoCodec::JPEG:
            key.codec = QnAvCodecHelper::codecIdToString(AV_CODEC_ID_MJPEG);
            break;
        case UnderstandableVideoCodec::H264:
            key.codec = QnAvCodecHelper::codecIdToString(AV_CODEC_ID_H264);
            break;
        case UnderstandableVideoCodec::H265:
            key.codec = QnAvCodecHelper::codecIdToString(AV_CODEC_ID_HEVC);
            break;
    }

    //key.codec = QnAvCodecHelper::codecIdToString(
    //    capabilities.isH264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MJPEG);

    StreamCapabilityMap result;
    for (const auto& resolution: capabilities.resolutions)
    {
        key.resolution = resolution;
        result.insert(key, nx::media::CameraStreamCapability());
    }
    return result;
#else
    // new version
    using namespace nx::mediaserver::resource;

    QnMutexLocker lock(&m_mutex);

    StreamCapabilityMap result;

    if (m_videoEncoderConfigOptionsList.empty())
        return result;

    // Set options to primary by default
    VideoEncoderConfigOptions* options = &m_videoEncoderConfigOptionsList[0];

    if (streamIndex == Qn::StreamIndex::secondary)
    {
        if (m_videoEncoderConfigOptionsList.size() < 2)
            return result;
        else
            options = &m_videoEncoderConfigOptionsList[1];
    }

    StreamCapabilityKey key;
    key.codec = QString::fromStdString(VideoCodecToString(options->encoder));

    for (const auto& resolution: options->resolutions)
    {
        key.resolution = resolution;
        result.insert(key, nx::media::CameraStreamCapability());
    }
    return result;
#endif
}

CameraDiagnostics::Result QnPlOnvifResource::initializeCameraDriver()
{
    if (getDeviceOnvifUrl().isEmpty())
    {
        return m_prevOnvifResultCode.errorCode != CameraDiagnostics::ErrorCode::noError
            ? m_prevOnvifResultCode
            : CameraDiagnostics::RequestFailedResult("getDeviceOnvifUrl", QString());
    }

    setCameraCapability(Qn::customMediaPortCapability, true);

    calcTimeDrift();
    updateFirmware();

    const QAuthenticator auth = getAuth();
    DeviceSoapWrapper deviceSoapWrapper(
        onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);
    CapabilitiesResp capabilitiesResponse;
    /*
     Warning! The capabilitiesResponse lifetime must be not more then deviceSoapWrapper lifetime,
     because DeviceSoapWrapper destructor destroys internals of CapabilitiesResp.
    */

    auto result = initOnvifCapabilitiesAndUrls(deviceSoapWrapper, &capabilitiesResponse); //< step 1
    if (!checkResultAndSetStatus(result))
        return result;

    result = initializeMedia(/*parameter is not used*/capabilitiesResponse); //< step 2
    if (!checkResultAndSetStatus(result))
        return result;

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    initializeAdvancedParameters(capabilitiesResponse); //< step 3

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    initializeIo(capabilitiesResponse); //< step 4

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    initializePtz(capabilitiesResponse); //< step 5

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    result = customInitialization(capabilitiesResponse);
    if (!checkResultAndSetStatus(result))
        return result;

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initOnvifCapabilitiesAndUrls(
    DeviceSoapWrapper& deviceSoapWrapper,
    CapabilitiesResp* outCapabilitiesResponse)
{

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    auto result = fetchOnvifCapabilities(deviceSoapWrapper, outCapabilitiesResponse);
    if (!result)
        return result;

    fillFullUrlInfo(*outCapabilitiesResponse);

    if (getMediaUrl().isEmpty())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("ONVIF media URL is not filled by camera"));
    }

    QString media2ServiceUrl;
    fetchOnvifMedia2Url(&media2ServiceUrl); //< We ignore the result,
    // because old devices may not support Device::getServices request

    setMedia2Url(media2ServiceUrl);

    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::initializeMedia(
    const CapabilitiesResp& /*onvifCapabilities*/)
{
    auto result = fetchAndSetVideoSource();
    if (!result)
        return result;

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    fetchAndSetAudioSource();

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    result = fetchAndSetVideoResourceOptions();
    if (!result)
        return result;

    result = fetchAndSetAudioResourceOptions();
    if (!result)
        return result;

    m_audioTransmitter = initializeTwoWayAudio();
    if (m_audioTransmitter)
        setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);

    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::initializePtz(
    const CapabilitiesResp& /*onvifCapabilities*/)
{
    const bool result = fetchPtzInfo();
    if (!result)
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("Fetch Onvif PTZ configurations."),
            lit("Can not fetch Onvif PTZ configurations."));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initializeIo(
    const CapabilitiesResp& onvifCapabilities)
{
    const QnResourceData resourceData = this->resourceData();
    m_inputPortCount = 0;
    m_isRelayOutputInversed = resourceData.value(QString("relayOutputInversed"), false);
    m_fixWrongInputPortNumber = resourceData.value(QString("fixWrongInputPortNumber"), false);
    //registering onvif event handler
    std::vector<QnPlOnvifResource::RelayOutputInfo> RelayOutputInfoList;
    fetchRelayOutputs(&RelayOutputInfoList);
    if (!RelayOutputInfoList.empty())
    {
        setCameraCapability(Qn::OutputPortCapability, true);
        // TODO #ak it's not clear yet how to get input port list for sure
        // (on DW cam getDigitalInputs returns nothing),
        // but all cameras I've seen have either both input & output or none.
        setCameraCapability(Qn::InputPortCapability, true);

        //resetting all ports states to inactive
        for (auto i = 0; i < RelayOutputInfoList.size(); ++i)
            setOutputPortStateNonSafe(0, QString::fromStdString(RelayOutputInfoList[i].token), false, 0);
    }

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    fetchRelayInputInfo(onvifCapabilities);

    static const auto kForcedInputCountParameter = "relayInputCountForced";
    const bool isInputCountForced = resourceData.contains(kForcedInputCountParameter);

    int forcedInputCount = 0;
    if (isInputCountForced)
        forcedInputCount = resourceData.value<int>(kForcedInputCountParameter, 0);

    if (resourceData.contains("clearInputsTimeoutSec"))
    {
        m_clearInputsTimeoutUSec =
            resourceData.value<int>("clearInputsTimeoutSec", 0) * 1000 * 1000;
    }

    auto generateInputPorts =
        [this](int portCount)
        {
            QnIOPortDataList result;
            if (portCount > MAX_IO_PORTS_PER_DEVICE)
            {
                NX_WARNING(
                    this,
                    lm("Device %1 (%2) reports too many input ports (%3).")
                        .args(getName(), getId(), portCount));

                return result;
            }

            for (int i = 1; i <= portCount; ++i)
            {
                QnIOPortData inputPort;
                inputPort.portType = Qn::PT_Input;
                inputPort.id = lit("%1").arg(i);
                inputPort.inputName = tr("Input %1").arg(i);
                result.push_back(inputPort);
            }

            return result;
        };

    auto inputCount = forcedInputCount;
    if (!isInputCountForced &&
        onvifCapabilities.Capabilities &&
        onvifCapabilities.Capabilities->Device &&
        onvifCapabilities.Capabilities->Device->IO &&
        onvifCapabilities.Capabilities->Device->IO->InputConnectors &&
        *onvifCapabilities.Capabilities->Device->IO->InputConnectors > 0)
    {
        inputCount = *onvifCapabilities.Capabilities
            ->Device
            ->IO
            ->InputConnectors;
    }

    auto allPorts = generateOutputPorts();
    const auto inputPorts = generateInputPorts(inputCount);

    m_inputPortCount = inputPorts.size();
    const auto outputPortCount = allPorts.size();

    allPorts.insert(allPorts.cend(), inputPorts.cbegin(), inputPorts.cend());
    setIoPortDescriptions(std::move(allPorts), /*needMerge*/ true);

    m_portNamePrefixToIgnore = resourceData.value<QString>("portNamePrefixToIgnore", QString());
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initializeAdvancedParameters(
    const CapabilitiesResp& /*onvifCapabilities*/)
{
    fetchAndSetAdvancedParameters();
    return CameraDiagnostics::NoErrorResult();
}

int QnPlOnvifResource::suggestBitrateKbps(
    const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    return strictBitrate(
        nx::mediaserver::resource::Camera::suggestBitrateKbps(streamParams, role), role);
}

int QnPlOnvifResource::strictBitrate(int bitrate, Qn::ConnectionRole role) const
{
    auto resData = resourceData();

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

    auto availableBitrates =
        resData.value<QnBitrateList>(availableBitratesParamName, QnBitrateList());

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

QSize QnPlOnvifResource::findSecondaryResolution(
    const QSize& primaryRes, const QList<QSize>& secondaryResList, double* matchCoeff)
{
    auto resData = resourceData();

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
            NX_WARNING(this,
                lm("findSecondaryResolution(): Wrong parameter format "
                   "(FORCED_SECONDARY_STREAM_RESOLUTION_PARAM_NAME) %1"),
                forcedSecondaryResolution);
        }
    }

    auto result = closestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION,
        getResolutionAspectRatio(primaryRes),
        SECONDARY_STREAM_MAX_RESOLUTION,
        secondaryResList,
        matchCoeff);

    return result;
}

void QnPlOnvifResource::setMaxFps(int f)
{
    setProperty(Qn::MAX_FPS_PARAM_NAME, f);
}

const QString QnPlOnvifResource::getAudioEncoderId() const
{
    QnMutexLocker lock(&m_mutex);
    return m_audioEncoderId;
}

const QString QnPlOnvifResource::getVideoSourceId() const
{
    QnMutexLocker lock(&m_mutex);
    return m_videoSourceId;
}

const QString QnPlOnvifResource::getAudioSourceId() const
{
    QnMutexLocker lock(&m_mutex);
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
    CameraDiagnostics::Result result = readDeviceInformation(
        commonModule(), onvifTimeouts(), getDeviceOnvifUrl(), auth, m_timeDrift, &extInfo);
    if (result)
    {
        if (getName().isEmpty())
            setName(extInfo.name);
        if (getModel().isEmpty())
            setModel(extInfo.model);
        if (getFirmware().isEmpty())
            setFirmware(extInfo.firmware);
        if (getMAC().isNull())
            setMAC(nx::utils::MacAddress(extInfo.mac));
        if (getVendor() == lit("ONVIF") && !extInfo.vendor.isNull())
            setVendor(extInfo.vendor); // update default vendor
        if (getPhysicalId().isEmpty())
        {
            QString id = extInfo.hardwareId;
            if (!extInfo.serial.isEmpty())
                id += QString(L'_') + extInfo.serial;
            setPhysicalId(id);
        }
    }
    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::readDeviceInformation(
    QnCommonModule* commonModule, const SoapTimeouts& onvifTimeouts,
    const QString& onvifUrl, const QAuthenticator& auth, int timeDrift, OnvifResExtInfo* extInfo)
{
    if (timeDrift == INT_MAX)
        timeDrift = calcTimeDrift(onvifTimeouts, onvifUrl);

    DeviceSoapWrapper soapWrapper(onvifTimeouts,
        onvifUrl.toStdString(), auth.user(), auth.password(), timeDrift);

    DeviceInfoReq request;
    DeviceInfoResp response;

    if (commonModule->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
            << soapWrapper.endpoint() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastErrorDescription();
#endif
        if (soapWrapper.lastErrorIsNotAuthenticated())
            return CameraDiagnostics::NotAuthorisedResult(onvifUrl);

        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getDeviceInformation"), soapWrapper.getLastErrorDescription());
    }
    else
    {
        extInfo->name = QString::fromStdString(response.Manufacturer) + QLatin1String(" - ")
            + QString::fromStdString(response.Model);
        extInfo->model = QString::fromStdString(response.Model);
        extInfo->firmware = QString::fromStdString(response.FirmwareVersion);
        extInfo->vendor = QString::fromStdString(response.Manufacturer);
        extInfo->hardwareId = QString::fromStdString(response.HardwareId);
        extInfo->serial = QString::fromStdString(response.SerialNumber);
    }

    if (commonModule->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    //Trying to get MAC
    NetIfacesReq requestIfList;
    NetIfacesResp responseIfList;

    // This request is optional.
    soapRes = soapWrapper.getNetworkInterfaces(requestIfList, responseIfList);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch MAC address. Reason: SOAP to endpoint "
            << onvifUrl << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastErrorDescription();
#endif
    }
    else
    {
        extInfo->mac = fetchMacAddress(responseIfList, QUrl(onvifUrl).host());
    }

    return CameraDiagnostics::NoErrorResult();
}

/*
    The following five functions are to parse the notification XML. Here's the example:
    <tt:Message UtcTime="2018-05-03T17:06:42Z" PropertyOperation="Changes">
        <tt:Source>
            <tt:SimpleItem Name="channel" Value="0"/>
            <tt:SimpleItem Name="additional" Value="left"/>
        </tt:Source>
        <tt:Key>
        </tt:Key>
        <tt:Data>
            <tt:SimpleItem Name="state" Value="false"/>
        </tt:Data>
    </tt:Message>
*/

const char*  QnPlOnvifResource::attributeTextByName(const soap_dom_element* element, const char* attributeName)
{
    NX_ASSERT(attributeName);
    // soap_dom_element methods have no const specifiers, so we are compelled to use const_cast
    soap_dom_attribute_iterator it = const_cast<soap_dom_element*>(element)->att_find(attributeName);
    const char* const text = (it != soap_dom_attribute_iterator())
        ? (it->text ? it->text : "")
        : "";
    NX_ASSERT(text);
    return text;
}

QnPlOnvifResource::onvifSimpleItem  QnPlOnvifResource::parseSimpleItem(const soap_dom_element* element)
{
    // if an element is a simple item, it has the only subelement
    // with attributes "Name" and "Value"
    const char* const name = attributeTextByName(element, "Name");
    const char* const value = attributeTextByName(element, "Value");
    return onvifSimpleItem(name, value);
}

QnPlOnvifResource::onvifSimpleItem  QnPlOnvifResource::parseChildSimpleItem(
    const soap_dom_element* element)
{
    if (element->elts)
        return parseSimpleItem(element->elts);
    else
        return onvifSimpleItem();
}

std::vector<QnPlOnvifResource::onvifSimpleItem>  QnPlOnvifResource::parseChildSimpleItems(
    const soap_dom_element* element)
{
    // if an element contains a simple item, it has the only subelement
    // with attributes "Name" and "Value"
    std::vector<onvifSimpleItem> result;
    for (soap_dom_element* subElement = element->elts; subElement; subElement = subElement->next)
    {

        result.emplace_back(parseSimpleItem(subElement));
    }
    return result;
}

void QnPlOnvifResource::parseSourceAndData(
    const soap_dom_element* element,
    std::vector<QnPlOnvifResource::onvifSimpleItem>* source,
    QnPlOnvifResource::onvifSimpleItem* data)
{
    static const QByteArray kSource = "Source";
    static const QByteArray kData = "Data";
    for (soap_dom_element* elt = element->elts; elt; elt = elt->next)
    {
        if (!elt->name)
            continue;
        const QByteArray name = QByteArray::fromRawData(elt->name, (int)strlen(elt->name));
        QByteArray pureName = name.split(':').back();
        soap_dom_element* subElement = elt->elts;
        if (pureName == kSource)
            *source = parseChildSimpleItems(elt);
        else if (pureName == kData)
            *data = parseChildSimpleItem(elt);
    }
}

void QnPlOnvifResource::notificationReceived(
    const oasisWsnB2__NotificationMessageHolderType& notification, time_t minNotificationTime)
{
    const auto now = qnSyncTime->currentUSecsSinceEpoch();
    if (m_clearInputsTimeoutUSec > 0)
    {
        for (auto& state: m_relayInputStates)
        {
            if (state.second.value && state.second.timestamp + m_clearInputsTimeoutUSec < now)
            {
                state.second.value = false;
                state.second.timestamp = now;
                onRelayInputStateChange(state.first, state.second);
            }
        }
    }

    if (!notification.Message.__any.name)
    {
        NX_VERBOSE(this, lit("Received notification with empty message. Ignoring..."));
        return;
    }

    if (!notification.Topic ||
        !notification.Topic->__any.text)
    {
        NX_VERBOSE(this, lit("Received notification with no topic specified. Ignoring..."));
        return;
    }

    QString eventTopic(QLatin1String(notification.Topic->__any.text));

    NX_VERBOSE(this, lit("%1 Received notification %2").arg(getUrl()).arg(eventTopic));

    // eventTopic may have namespaces. E.g., ns:Device/ns:Trigger/ns:Relay,
    // but we want Device/Trigger/Relay. Fixing...
    QStringList eventTopicTokens = eventTopic.split(QLatin1Char('/'));
    for(QString& token: eventTopicTokens)
    {
        int nsSepIndex = token.indexOf(QLatin1Char(':'));
        if (nsSepIndex != -1)
            token.remove(0, nsSepIndex+1);
    }
    eventTopic = eventTopicTokens.join(QLatin1Char('/'));

    if (eventTopic.indexOf(lit("Trigger/Relay")) == -1 &&
        eventTopic.indexOf(lit("IO/Port")) == -1 &&
        eventTopic.indexOf(lit("Trigger/DigitalInput")) == -1 &&
        eventTopic.indexOf(lit("Device/IO/VirtualPort")) == -1)
    {
        NX_VERBOSE(this, lit("Received notification with unknown topic: %1. Ignoring...").
            arg(QLatin1String(notification.Topic->__any.text)));
        return;
    }

    //parsing Message
    QString text = notification.Message.__any.atts->text; // for example: "2018-04-30T21:18:37Z"
    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (dateTime.timeSpec() == Qt::LocalTime)
        dateTime.setTimeZone(m_cameraTimeZone);
    if (dateTime.timeSpec() != Qt::UTC)
        dateTime = dateTime.toUTC();

    if ((minNotificationTime != (time_t)-1) && (dateTime.toTime_t() < minNotificationTime))
        return; //ignoring old notifications: DW camera can deliver old cached notifications

    std::vector<onvifSimpleItem> source;
    onvifSimpleItem data;
    parseSourceAndData(&notification.Message.__any, &source, &data);

    bool sourceIsExplicitRelayInput = false;

    //checking that there is single source and this source is a relay port
    auto portSourceIter = source.cend();
    for (auto it = source.cbegin(); it != source.cend(); ++it)
    {
        if (it->name == "port" ||
            it->name == "RelayToken" ||
            it->name == "Index")
        {
            portSourceIter = it;
            break;
        }
        else if (it->name == "InputToken" || it->name == "RelayInputToken")
        {
            sourceIsExplicitRelayInput = true;
            portSourceIter = it;
            break;
        }
    }

    if (portSourceIter == /*handler.*/source.end()  //< source is not port
        || data.name != "LogicalState" &&
           data.name != "state" &&
           data.name != "Level" &&
           data.name != "RelayLogicalState")
    {
        return;
    }
    const QString portSourceValue = QString::fromStdString(portSourceIter->value);

    // Some cameras (especially, Vista) send here events on output port, filtering them out.
    // And some cameras, e.g. DW-PF5M1TIR correctly send here events on input port,
    // but set port number to output port, so to distinguish these two situations we use
    // "fixWrongInputPortNumber" parameter from resource_data.json.

    const bool sourceNameMatchesRelayOutPortName =
        std::find_if(
            m_relayOutputInfo.begin(),
            m_relayOutputInfo.end(),
            [&source](const RelayOutputInfo& outputInfo)
            {
                return outputInfo.token == source.front().value;
            }) != m_relayOutputInfo.end();

    const bool sourceNameHasPrefixToIgnore = (!m_portNamePrefixToIgnore.isEmpty()
         && QString::fromStdString(source.front().value).startsWith(m_portNamePrefixToIgnore));

    const bool sourceIsRelayOutPort = sourceNameHasPrefixToIgnore
        || (sourceNameMatchesRelayOutPortName && !m_fixWrongInputPortNumber);

    if (!sourceIsExplicitRelayInput && !/*handler.*/source.empty() && sourceIsRelayOutPort)
        return; //< This is notification about output port.

    // saving port state
    const bool newValue = (data.value == "true")
        || (data.value == "active") || (atoi(data.value.c_str()) > 0);
    auto& state = m_relayInputStates[portSourceValue];
    state.timestamp = now;
    if (state.value != newValue)
    {
        state.value = newValue;
        onRelayInputStateChange(portSourceValue, state);
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
    emit inputPortStateChanged(
        toSharedPointer(),
        portId,
        state.value,
        state.timestamp);
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoResourceOptions()
{

    CameraDiagnostics::Result result = fetchAndSetVideoEncoderOptions();
    if (!result)
        return result;

    //result = fetchAndSetVideoEncoderOptionsNew();
    //if (!result)
    //    return result;

    result = updateResourceCapabilities();
    if (!result)
        return result;

    // Before invoking <fetchAndSetHasDualStreaming> Primary and Secondary Resolutions MUST be set.
    fetchAndSetDualStreaming();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetAudioResourceOptions()
{
    MediaSoapWrapper soapWrapper(this);

    if (fetchAndSetAudioEncoder(soapWrapper) && fetchAndSetAudioEncoderOptions(soapWrapper))
        setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, 1);
    else
        setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, QString(lit("0")));

    return CameraDiagnostics::NoErrorResult();
}

int QnPlOnvifResource::innerQualityToOnvif(
    Qn::StreamQuality quality, int minQuality, int maxQuality) const
{
    if (quality > Qn::StreamQuality::highest)
    {
        NX_VERBOSE(this,
            lm("innerQualityToOnvif: got unexpected quality (too big): %1")
            .arg((int)quality));
        return maxQuality;
    }
    if (quality < Qn::StreamQuality::lowest)
    {
        NX_VERBOSE(this,
            lm("innerQualityToOnvif: got unexpected quality (too small): %1")
            .arg((int)quality));
        return minQuality;
    }

    int onvifQuality = minQuality
        + (maxQuality - minQuality)
        * ((int)quality - (int)Qn::StreamQuality::lowest)
        / ((int)Qn::StreamQuality::highest - (int)Qn::StreamQuality::lowest);
    NX_DEBUG(this, QString(lit("innerQualityToOnvif: in quality = %1, out qualty = %2, minOnvifQuality = %3, maxOnvifQuality = %4"))
            .arg((int)quality)
            .arg(onvifQuality)
            .arg(minQuality)
            .arg(maxQuality));

    return onvifQuality;
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

    if (soapRes != SOAP_OK && soapWrapper.lastErrorIsNotAuthenticated())
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
    m_timeDrift = calcTimeDrift(onvifTimeouts(), getDeviceOnvifUrl(), outSoapRes, &m_cameraTimeZone);
    m_timeDriftTimer.restart();
}

int QnPlOnvifResource::calcTimeDrift(
    const SoapTimeouts& timeouts,
    const QString& deviceUrl, int* outSoapRes, QTimeZone* timeZone)
{
    DeviceSoapWrapper soapWrapper(timeouts, deviceUrl.toStdString(), QString(), QString(), 0);

    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = soapWrapper.GetSystemDateAndTime(request, response);
    if (outSoapRes)
        *outSoapRes = soapRes;

    if (soapRes == SOAP_OK && response.SystemDateAndTime
        && response.SystemDateAndTime->UTCDateTime)
    {
        if (timeZone && response.SystemDateAndTime->TimeZone)
            *timeZone = QTimeZone(response.SystemDateAndTime->TimeZone->TZ.c_str());

        onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
        onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;
        if (!date || !time)
            return 0;

        QDateTime datetime(
            QDate(date->Year, date->Month, date->Day),
            QTime(time->Hour, time->Minute, time->Second),
            Qt::UTC);
        int drift = datetime.toMSecsSinceEpoch()/MS_PER_SECOND
            - QDateTime::currentMSecsSinceEpoch()/MS_PER_SECOND;
        return drift;
    }
    return 0;
}

QString QnPlOnvifResource::getMediaUrl() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serviceUrls.mediaServiceUrl;
}

void QnPlOnvifResource::setMediaUrl(const QString& src)
{
    QnMutexLocker lock(&m_mutex);
    m_serviceUrls.mediaServiceUrl = src;
}

QString QnPlOnvifResource::getMedia2Url() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serviceUrls.media2ServiceUrl;
}

void QnPlOnvifResource::setMedia2Url(const QString& src)
{
    QnMutexLocker lock(&m_mutex);
    m_serviceUrls.media2ServiceUrl = src;
}

QString QnPlOnvifResource::getImagingUrl() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serviceUrls.imagingServiceUrl;
}

void QnPlOnvifResource::setImagingUrl(const QString& src)
{
    QnMutexLocker lock(&m_mutex);
    m_serviceUrls.imagingServiceUrl = src;
}

QString QnPlOnvifResource::getDeviceIOUrl() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serviceUrls.deviceioServiceUrl;
}

void QnPlOnvifResource::setDeviceIOUrl(const QString& src)
{
    QnMutexLocker lock(&m_mutex);
    m_serviceUrls.deviceioServiceUrl = src;
}

QString QnPlOnvifResource::getVideoSourceToken() const
{
    QnMutexLocker lock(&m_mutex);
    return m_videoSourceToken;
}

void QnPlOnvifResource::setVideoSourceToken(const QString &src)
{
    QnMutexLocker lock(&m_mutex);
    m_videoSourceToken = src;
}

QString QnPlOnvifResource::getPtzUrl() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serviceUrls.ptzServiceUrl;
}

void QnPlOnvifResource::setPtzUrl(const QString& src)
{
    QnMutexLocker lock(&m_mutex);
    m_serviceUrls.ptzServiceUrl = src;
}

QString QnPlOnvifResource::getPtzConfigurationToken() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ptzConfigurationToken;
}

void QnPlOnvifResource::setPtzConfigurationToken(const QString &src)
{
    QnMutexLocker lock(&m_mutex);
    m_ptzConfigurationToken = src;
}

QString QnPlOnvifResource::getPtzProfileToken() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ptzProfileToken;
}

void QnPlOnvifResource::setPtzProfileToken(const QString& src)
{
    QnMutexLocker lock(&m_mutex);
    m_ptzProfileToken = src;
}

bool QnPlOnvifResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source)
{
    QnPlOnvifResourcePtr onvifR = source.dynamicCast<QnPlOnvifResource>();
    if (!onvifR)
        return false;

    bool result = nx::mediaserver::resource::Camera::mergeResourcesIfNeeded(source);

    QString onvifUrlSource = onvifR->getDeviceOnvifUrl();
    if (!onvifUrlSource.isEmpty() && getDeviceOnvifUrl() != onvifUrlSource)
    {
        setDeviceOnvifUrl(onvifUrlSource);
        result = true;
    }

    return result;
}

static QString getRelayOutpuToken(const QnPlOnvifResource::RelayOutputInfo& relayInfo)
{
    return QString::fromStdString(relayInfo.token);
}

QnIOPortDataList QnPlOnvifResource::generateOutputPorts() const
{
    QStringList idList;
    std::transform(
        m_relayOutputInfo.begin(),
        m_relayOutputInfo.end(),
        std::back_inserter(idList),
        getRelayOutpuToken);
    QnIOPortDataList result;
    for (const auto& data: idList)
    {
        QnIOPortData value;
        value.portType = Qn::PT_Output;
        value.id = data;
        value.outputName = tr("Output %1").arg(data);
        result.push_back(value);
    }
    return result;
}

bool QnPlOnvifResource::fetchRelayInputInfo(const CapabilitiesResp& capabilitiesResponse)
{
    if (getDeviceIOUrl().isEmpty())
        return false;

    if (capabilitiesResponse.Capabilities
        && capabilitiesResponse.Capabilities->Device
        && capabilitiesResponse.Capabilities->Device->IO
        && capabilitiesResponse.Capabilities->Device->IO->InputConnectors
        && *capabilitiesResponse.Capabilities->Device->IO->InputConnectors > 0
        && *capabilitiesResponse.Capabilities->Device->IO->InputConnectors
        < (int) MAX_IO_PORTS_PER_DEVICE)
    {
        // Camera has input port.
        setCameraCapability(Qn::InputPortCapability, true);
    }

    auto resData = resourceData();
    m_portAliases = resData.value<std::vector<QString>>(Qn::ONVIF_INPUT_PORT_ALIASES_PARAM_NAME);

    if (!m_portAliases.empty())
        return true;

    DeviceIO::DigitalInputs digitalInputs(this);
    digitalInputs.receiveBySoap();

    if (!digitalInputs && digitalInputs.soapError() != SOAP_MUSTUNDERSTAND)
    {
        NX_DEBUG(this, lit("Failed to get relay digital input list. endpoint %1")
            .arg(digitalInputs.endpoint()));
        return true;
    }

    m_portAliases.clear();
    for (const auto& input: digitalInputs.get()->DigitalInputs)
        m_portAliases.push_back(QString::fromStdString(input->token));

    return true;
}

bool QnPlOnvifResource::fetchPtzInfo()
{
    PtzSoapWrapper ptz(this);
    if (!ptz)
    {
        // #TODO: log.
        return false;
    }
    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) == SOAP_OK
        && response.PTZConfiguration.size() > 0)
    {
        m_ptzConfigurationToken = QString::fromStdString(response.PTZConfiguration[0]->token);
    }
    return true;
}

//!Implementation of QnSecurityCamResource::setOutputPortState
bool QnPlOnvifResource::setOutputPortState(
    const QString& outputID,
    bool active,
    unsigned int autoResetTimeoutMS)
{
    QnMutexLocker lk(&m_ioPortMutex);

    using namespace std::placeholders;
    const quint64 timerID = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&QnPlOnvifResource::setOutputPortStateNonSafe, this, _1, outputID, active,
            autoResetTimeoutMS),
        std::chrono::milliseconds::zero());
    m_triggerOutputTasks[timerID] = TriggerOutputTask(outputID, active, autoResetTimeoutMS);
    return true;
}

boost::optional<onvifXsd__H264Profile> QnPlOnvifResource::getH264StreamProfile(
    const VideoOptionsLocal& videoOptionsLocal)
{
    auto resData = resourceData();

    auto desiredH264Profile = resData.value<QString>(Qn::DESIRED_H264_PROFILE_PARAM_NAME);

    if (videoOptionsLocal.h264Profiles.isEmpty())
        return boost::optional<onvifXsd__H264Profile>();
    else if (!desiredH264Profile.isEmpty())
        return fromStringToH264Profile(desiredH264Profile);
    else
        return videoOptionsLocal.h264Profiles[0];
}

qreal QnPlOnvifResource::getBestSecondaryCoeff(const QList<QSize> resList, qreal aspectRatio) const
{
    int maxSquare =
        SECONDARY_STREAM_MAX_RESOLUTION.width() * SECONDARY_STREAM_MAX_RESOLUTION.height();
    QSize secondaryRes = getNearestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION, aspectRatio, maxSquare, resList);
    if (secondaryRes == EMPTY_RESOLUTION_PAIR)
    {
        secondaryRes =
            getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0, maxSquare, resList);
    }
    qreal secResSquare =
        SECONDARY_STREAM_DEFAULT_RESOLUTION.width() * SECONDARY_STREAM_DEFAULT_RESOLUTION.height();
    qreal findResSquare = secondaryRes.width() * secondaryRes.height();
    if (findResSquare > secResSquare)
        return findResSquare / secResSquare;
    else
        return secResSquare / findResSquare;
}

// #########################
// Need to be revised.
// #########################
int QnPlOnvifResource::getSecondaryIndex(const QList<VideoOptionsLocal>& optList) const
{
    if (optList.size() < 2 || optList[0].resolutions.isEmpty())
        return 1; // default value

    qreal bestResCoeff = INT_MAX;
    int bestResIndex = 1;
    bool bestIsDesirable = false;

    const qreal aspectRatio = (qreal) optList[0].resolutions[0].width()
        / (qreal) optList[0].resolutions[0].height();

    for (int i = 1; i < optList.size(); ++i)
    {
        qreal resCoeff = getBestSecondaryCoeff(optList[i].resolutions, aspectRatio);
        if (resCoeff < bestResCoeff
            || (resCoeff == bestResCoeff
                && optList[i].encoding == UnderstandableVideoCodec::Desirable
                && !bestIsDesirable))
        {
            bestResCoeff = resCoeff;
            bestResIndex = i;
            bestIsDesirable = (optList[i].encoding == UnderstandableVideoCodec::Desirable);
        }
    }

    return bestResIndex;
}

bool QnPlOnvifResource::registerNotificationConsumer()
{
    if (commonModule()->isNeedToStop())
        return false;

    QnMutexLocker lk(&m_ioPortMutex);

    //determining local address, accessible by onvif device
    QUrl eventServiceURL(QString::fromStdString(m_eventCapabilities->XAddr));

    // TODO: #ak should read local address only once
    std::unique_ptr<nx::network::AbstractStreamSocket> sock(
        nx::network::SocketFactory::createStreamSocket());
    if(!sock->connect(
            eventServiceURL.host(), eventServiceURL.port(nx::network::http::DEFAULT_HTTP_PORT),
            nx::network::deprecated::kDefaultConnectTimeout))
    {
        NX_WARNING(this, lit("Failed to connect to %1:%2 to determine local address. %3")
            .arg(eventServiceURL.host()).arg(eventServiceURL.port())
            .arg(SystemError::getLastOSErrorText()));
        return false;
    }
    QString localAddress = sock->getLocalAddress().address.toString();

    QAuthenticator auth = getAuth();

    NotificationProducerSoapWrapper soapWrapper(
        onvifTimeouts(),
        m_eventCapabilities->XAddr,
        auth.user(),
        auth.password(),
        m_timeDrift);
    soapWrapper.soap()->imode |= SOAP_XML_IGNORENS;

    char buf[512];

    //providing local gsoap server url
    _oasisWsnB2__Subscribe request;

    sprintf(buf, "http://%s:%d%s", localAddress.toLatin1().data(),
        QnSoapServer::instance()->port(), QnSoapServer::instance()->path().toLatin1().data());

    request.ConsumerReference.Address = buf;

    //setting InitialTerminationTime (if supported)
    sprintf(buf, "PT%dS", DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT);
    std::string initialTerminationTime(buf);
    request.InitialTerminationTime = &initialTerminationTime;

    //creating filter
    //oasisWsnB2__FilterType topicFilter;
    //strcpy(buf, "<wsnt:TopicExpression Dialect=\"xsd:anyURI\">tns1:Device/Trigger/Relay</wsnt:TopicExpression>");
    //topicFilter.__any.push_back(buf);
    //request.Filter = &topicFilter;

    _oasisWsnB2__SubscribeResponse response;
    const int soapCallResult = soapWrapper.subscribe(&request, &response);

    // TODO/IMPL: find out which is error and which is not.
    if (soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND)
    {
        NX_WARNING(this, lit("Failed to subscribe in NotificationProducer. endpoint %1")
            .arg(QString::fromLatin1(soapWrapper.endpoint())));
        return false;
    }

    NX_VERBOSE(this, lit("%1 subscribed to notifications").arg(getUrl()));

    if (commonModule()->isNeedToStop())
        return false;

    // TODO: #ak if this variable is unused following code may be deleted as well
    time_t utcTerminationTime; // = ::time(NULL) + DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;
    if (response.TerminationTime)
    {
        if (response.CurrentTime)
        {
            utcTerminationTime = ::time(NULL)
                + *response.TerminationTime
                - *response.CurrentTime;
        }
        else
        {
            // Hoping local and cam clocks are synchronized.
            utcTerminationTime = *response.TerminationTime;
        }
    }
    // Else: considering, that onvif device processed initialTerminationTime.
    Q_UNUSED(utcTerminationTime)

    std::string subscriptionID;
    {
        if (response.SubscriptionReference.ReferenceParameters &&
            response.SubscriptionReference.ReferenceParameters->__any)
        {
            //parsing to retrieve subscriptionId. Example: "<dom0:SubscriptionId xmlns:dom0=\"(null)\">0</dom0:SubscriptionId>"
            QXmlSimpleReader reader;
            SubscriptionReferenceParametersParseHandler handler;
            reader.setContentHandler(&handler);
            QBuffer srcDataBuffer;
            srcDataBuffer.setData(
                response.SubscriptionReference.ReferenceParameters->__any[0],
                (int)strlen(response.SubscriptionReference.ReferenceParameters->__any[0]));
            QXmlInputSource xmlSrc(&srcDataBuffer);
            if (reader.parse(&xmlSrc))
                m_onvifNotificationSubscriptionID = handler.subscriptionID;
        }

        if (response.SubscriptionReference.Address)
        {
            m_onvifNotificationSubscriptionReference =
                fromOnvifDiscoveredUrl(response.SubscriptionReference.Address);
        }
    }

    // Launching renew-subscription timer.
    unsigned int renewSubsciptionTimeoutSec = 0;
    if (response.CurrentTime && response.TerminationTime)
    {
        renewSubsciptionTimeoutSec =
            *response.TerminationTime - *response.CurrentTime;
    }
    else
        renewSubsciptionTimeoutSec = DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;

    scheduleRenewSubscriptionTimer(renewSubsciptionTimeoutSec);

    if (commonModule()->isNeedToStop())
        return false;

    /* Note that we don't pass shared pointer here as this would create a
     * cyclic reference and onvif resource will never be deleted. */
    QnSoapServer::instance()->getService()->registerResource(
        toSharedPointer(this),
        QUrl(QString::fromStdString(m_eventCapabilities->XAddr)).host(),
        m_onvifNotificationSubscriptionReference);

    m_eventMonitorType = emtNotification;

    NX_DEBUG(this, lit("Successfully registered in NotificationProducer. endpoint %1")
        .arg(QString::fromLatin1(soapWrapper.endpoint())));
    return true;
}

void QnPlOnvifResource::scheduleRenewSubscriptionTimer(unsigned int timeoutSec)
{
    if (timeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS)
        timeoutSec -= RENEW_NOTIFICATION_FORWARDING_SECS;

    const std::chrono::seconds timeout(timeoutSec);
    NX_VERBOSE(this, lm("Schedule renew subscription in %1").arg(timeout));
    updateTimer(&m_renewSubscriptionTimerID, timeout,
        [this](quint64 timerID) { this->onRenewSubscriptionTimer(timerID); });
        //std::bind(&QnPlOnvifResource::onRenewSubscriptionTimer, this, std::placeholders::_1));
}

CameraDiagnostics::Result QnPlOnvifResource::updateVideoEncoderUsage(
    QList<VideoOptionsLocal>& optionsList)
{
    Media::Profiles profiles(this);
    profiles.receiveBySoap();
    if (!profiles)
        return profiles.requestFailedResult();

    for (const onvifXsd__Profile* profile: profiles.get()->Profiles)
    {
        if (profile->token.empty() || !profile->VideoEncoderConfiguration)
            continue;
        QString vEncoderID = QString::fromStdString(profile->VideoEncoderConfiguration->token);
        for (int i = 0; i < optionsList.size(); ++i)
        {
            if (optionsList[i].id == vEncoderID)
            {
                optionsList[i].usedInProfiles = true;
                optionsList[i].currentProfile = QString::fromStdString(profile->Name);
            }
        }
    }
    return CameraDiagnostics::NoErrorResult();
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
    bool result = resourceData().value<bool>(QString("trustMaxFPS"), false);
    return result;
}

bool QnPlOnvifResource::getVideoEncoderTokens(
    const std::vector<onvifXsd__VideoEncoderConfiguration*>& configurations,
    QStringList* tokenList)
{
    int confRangeStart = 0;
    int confRangeEnd = (int)configurations.size();
    if (m_maxChannels > 1)
    {
        const int configurationsPerChannel = (int)configurations.size() / m_maxChannels;
        confRangeStart = configurationsPerChannel * getChannel();
        confRangeEnd = confRangeStart + configurationsPerChannel;

        if (confRangeEnd > (int)configurations.size())
        {
#ifdef PL_ONVIF_DEBUG
            qWarning() << "invalid channel number " << getChannel() + 1
                << "for camera" << getHostAddress() << "max channels=" << m_maxChannels;
#endif
            return false;

        }
    }

    for (int i = confRangeStart; i < confRangeEnd; ++i)
    {
        const onvifXsd__VideoEncoderConfiguration* configuration = configurations[i];
        if (configuration)
            tokenList->push_back(QString::fromStdString(configuration->token));
    }

    return true;
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

CameraDiagnostics::Result QnPlOnvifResource::ReadVideoEncoderOptionsForToken(
    const std::string& token, QList<VideoOptionsLocal>* dstOptionsList, const QnBounds& frameRateBounds)
{
    if (m_serviceUrls.media2ServiceUrl.isEmpty())
    {
        // Old code - Media.
        _onvifMedia__GetVideoEncoderConfigurationOptions request;
        request.ConfigurationToken = const_cast<std::string*>(&token);

        Media::VideoEncoderConfigurationOptions videoEncoderConfigurationOptions(this);
        videoEncoderConfigurationOptions.receiveBySoap(request);

        if (!videoEncoderConfigurationOptions)
        {
            // #TODO log
            return videoEncoderConfigurationOptions.requestFailedResult();
        }
        const onvifXsd__VideoEncoderConfigurationOptions* options =
            videoEncoderConfigurationOptions.get()->Options;
        if (!options)
        {
            // Soap data receiving succeeded, but no options-data available.
            // #TODO log
            return CameraDiagnostics::NoErrorResult();
        }

        if (!options->H264 && !options->JPEG)
        {
            // Soap data receiving succeeded, but no needed options-data available.
            // #TODO log
            return CameraDiagnostics::NoErrorResult();
        }

        *dstOptionsList << VideoOptionsLocal(
            QString::fromStdString(token), *options, frameRateBounds);
}
    else
    {
        // New code - Media2.

        Media2::VideoEncoderConfigurationOptions videoEncoderConfigurationOptions(this);
        Media2::VideoEncoderConfigurationOptions::Request request;
        request.ConfigurationToken = const_cast<std::string*>(&token);
        videoEncoderConfigurationOptions.receiveBySoap(request);

        if (!videoEncoderConfigurationOptions)
        {
            // #TODO log
            return videoEncoderConfigurationOptions.requestFailedResult();
        }

        std::vector<onvifXsd__VideoEncoder2ConfigurationOptions *>& optionsList =
            videoEncoderConfigurationOptions.get()->Options;

        for (const onvifXsd__VideoEncoder2ConfigurationOptions* options: optionsList)
        {
            if (options && VideoCodecFromString(options->Encoding) != UnderstandableVideoCodec::NONE)
            {
                *dstOptionsList << VideoOptionsLocal(QString::fromStdString(token), *options, frameRateBounds);
            }
        }

    }
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoEncoderOptions()
{
    // Step 1. Get videoEncoderConfigurations and videoEncodersTokenList
    Media::VideoEncoderConfigurations videoEncoderConfigurations(this);

    QnOnvifConfigDataPtr forcedOnvifParams =
        resourceData().value<QnOnvifConfigDataPtr>(QString("forcedOnvifParams"));
    QStringList videoEncodersTokenList;

    if (forcedOnvifParams && forcedOnvifParams->videoEncoders.size() > getChannel())
    {
        videoEncodersTokenList = forcedOnvifParams->videoEncoders[getChannel()].split(L',');
    }
    else
    {
            if (commonModule()->isNeedToStop())
            return CameraDiagnostics::ServerTerminatedResult();

        videoEncoderConfigurations.receiveBySoap();
        if (!videoEncoderConfigurations)
        {
            // LOG.
            return videoEncoderConfigurations.requestFailedResult();
        }

        auto result = getVideoEncoderTokens(
            videoEncoderConfigurations.get()->Configurations, &videoEncodersTokenList);
        if (!result)
        {
            // LOG.
            return videoEncoderConfigurations.requestFailedResult();
        }
    }

    // Step 2. Extract video encoder options for every token into optionsList.
    const auto frameRateBounds = resourceData().value<QnBounds>(Qn::FPS_BOUNDS_PARAM_NAME, QnBounds());
    QList<VideoOptionsLocal> optionsList;
    for(const QString& encoderToken: videoEncodersTokenList)
    {
        CameraDiagnostics::Result result(CameraDiagnostics::ErrorCode::unknown);

        // Usually we make the only attempt,
        // but for some camera manufactures there are up to 5 attempts.
        const int attemptsCount = getMaxOnvifRequestTries();
        for (int i = 0; i < attemptsCount; ++i)
        {
            if (commonModule()->isNeedToStop())
                return CameraDiagnostics::ServerTerminatedResult();
            std::string token = encoderToken.toStdString();
            result = ReadVideoEncoderOptionsForToken(token, &optionsList, frameRateBounds);
            if (result)
                break;
        }

        if (!result)
            return result;
    }

    if (optionsList.isEmpty())
    {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: all video options are empty. (URL: "
            << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId() << ").";
#endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("fetchAndSetVideoEncoderOptions"), QLatin1String("no video options"));
    }

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    // Step 3. Mark options, that are used in profiles
    CameraDiagnostics::Result result = updateVideoEncoderUsage(optionsList);
    if (!result)
        return result;

    // Step 4. Sort options (i.e. video encoder configurations)
    auto profileNameLists = forcedOnvifParams ? forcedOnvifParams->profiles : QVector<QString>();
    const int channel = getChannel();
    QString channelProfileNameList;

    if (profileNameLists.size() > channel)
        channelProfileNameList = profileNameLists[channel];

    auto comparator = createComparator(channelProfileNameList);
    std::sort(optionsList.begin(), optionsList.end(), comparator);

    if (optionsList[0].frameRateMax > 0)
        setMaxFps(optionsList[0].frameRateMax);
    if (m_maxChannels == 1 && !trustMaxFPS() && !isCameraControlDisabled())
    {

        onvifXsd__VideoEncoderConfiguration* bestConfiguration = nullptr;
        auto& configurations = videoEncoderConfigurations.get()->Configurations;
        for (onvifXsd__VideoEncoderConfiguration* configuration: configurations)
        {
            if (configuration && QString::fromStdString(configuration->token) == optionsList[0].id)
            {
                bestConfiguration = configuration;
                break;
            }
        }
        if(bestConfiguration)
            checkMaxFps(bestConfiguration);
    }

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    {
        QnMutexLocker lock(&m_mutex);
        m_primaryStreamCapabilities = optionsList[0];
    }

    // Now we erase from optionList all but the first options
    // that correspond to the primary encoder configuration
    auto it = optionsList.begin();
    ++it;
    while (it != optionsList.end())
    {
        if (it->id == m_primaryStreamCapabilities.id)
            it = optionsList.erase(it);
        else
            ++it;
    }

    NX_DEBUG(this, QString(lit("ONVIF debug: got %1 encoders for camera %2"))
        .arg(videoEncodersTokenList.size()).arg(getHostAddress()));

    const bool dualStreamingAllowed = optionsList.size() >= 2;

    QnMutexLocker lock(&m_mutex);
    m_secondaryStreamCapabilities = VideoOptionsLocal();
    if (dualStreamingAllowed)
    {
        const int secondaryIndex = channelProfileNameList.isEmpty()
            ? getSecondaryIndex(optionsList)
            : 1;
        m_secondaryStreamCapabilities = optionsList[secondaryIndex];
    }

    return CameraDiagnostics::NoErrorResult();
}

//CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoEncoderOptionsNew()
//{
//    const QAuthenticator auth = getAuth();
//    int soapRes = SOAP_OK;
//    QString error;
//
//    if (m_serviceUrls.media2ServiceUrl.isEmpty())
//    {
//        MediaSoapWrapper soapWrapper(
//            onvifTimeouts(),
//            getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);
//
//        VideoOptionsReq optRequest;
//        VideoOptionsResp optResp;
//
//        soapRes = soapWrapper.getVideoEncoderConfigurationOptions(optRequest, optResp);
//        if (soapRes == SOAP_OK)
//            m_videoEncoderConfigOptionsList = VideoEncoderConfigOptionsList(optResp);
//        else
//            error = soapWrapper.getLastErrorDescription();
//    }
//    else
//    {
//        Media2SoapWrapper soapWrapper2(
//            onvifTimeouts(),
//            getMedia2Url().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);
//        //onvifMedia2__GetConfiguration optRequest2;
//        VideoOptionsResp2 optResp2;
//
//        soapRes = soapWrapper2.getVideoEncoderConfigurationOptions(optResp2);
//        if (soapRes == SOAP_OK)
//            m_videoEncoderConfigOptionsList = VideoEncoderConfigOptionsList(optResp2);
//        else
//            error = soapWrapper2.getLastErrorDescription();
//    }
//    if (soapRes == SOAP_OK)
//        return CameraDiagnostics::NoErrorResult();
//    else
//        return CameraDiagnostics::RequestFailedResult(
//            QLatin1String("getVideoEncoderConfigurationOptionsNew"), error);
//}

bool QnPlOnvifResource::fetchAndSetDualStreaming()
{
    QnMutexLocker lock(&m_mutex);

    auto resData = resourceData();

    bool forceSingleStream = resData.value<bool>(Qn::FORCE_SINGLE_STREAM_PARAM_NAME, false);

    const bool dualStreaming =
        !forceSingleStream
        && !m_secondaryStreamCapabilities.resolutions.isEmpty()
        && !m_secondaryStreamCapabilities.id.isEmpty();

    if (dualStreaming)
    {
        NX_DEBUG(this, lit("ONVIF debug: enable dualstreaming for camera %1")
                .arg(getHostAddress()));
    }
    else
    {
        QString reason =
            forceSingleStream ?
                lit("single stream mode is forced by driver") :
            m_secondaryStreamCapabilities.resolutions.isEmpty() ?
                lit("no secondary resolution") :
                QLatin1String("no secondary encoder");

        NX_DEBUG(this, lit("ONVIF debug: disable dualstreaming for camera %1 reason: %2")
                .arg(getHostAddress())
                .arg(reason));
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
    if (soapRes != SOAP_OK || !response.Options)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastErrorDescription();
#endif
        return false;

    }

    AUDIO_CODEC codec = AUDIO_NONE;
    AudioOptions* options = NULL;

    std::vector<AudioOptions*>::const_iterator it = response.Options->Options.begin();

    while (it != response.Options->Options.end())
    {
        AudioOptions* curOpts = *it;
        if (curOpts)
        {
            switch (curOpts->Encoding)
            {
                case onvifXsd__AudioEncoding::G711:
                    if (codec < G711)
                    {
                        codec = G711;
                        options = curOpts;
                    }
                    break;
                case onvifXsd__AudioEncoding::G726:
                    if (codec < G726)
                    {
                        codec = G726;
                        options = curOpts;
                    }
                    break;
                case onvifXsd__AudioEncoding::AAC:
                    if (codec < AAC)
                    {
                        codec = AAC;
                        options = curOpts;
                    }
                    break;
                case onvifXsd__AudioEncoding::AMR:
                    if (codec < AMR)
                    {
                        codec = AMR;
                        options = curOpts;
                    }
                    break;
                default:
#ifdef PL_ONVIF_DEBUG
                    qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: got unknown codec type: "
                        << curOpts->Encoding << " (URL: " << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                        << "). Root cause: SOAP request failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastErrorDescription();
#endif
                    break;
            }
        }

        ++it;
    }

    if (!options)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return data for G711, G726 or ACC (URL: "
            << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastErrorDescription();
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
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return Bitrate List (UniqueId: "
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
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return Samplerate List (UniqueId: "
            << getUniqueId() << ").";
    }
#endif

    {
        QnMutexLocker lock(&m_mutex);
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
        if (*it == threshold)
        {
            return *it;
        }

        if (*it < threshold && *it > floor)
        {
            floor = *it;
        } else if (*it > threshold && *it < ceil)
        {
            ceil = *it;
        }

        ++it;
    }

    if (floor < threshold)
    {
        return floor;
    }

    if (ceil > threshold)
    {
        return ceil;
    }

    return 0;
}

CameraDiagnostics::Result QnPlOnvifResource::updateResourceCapabilities()
{
    QnMutexLocker lock(&m_mutex);

    auto resData = resourceData();

    if (!m_videoSourceSize.isValid())
        return CameraDiagnostics::NoErrorResult();

    NX_DEBUG(this, QString(lit("ONVIF debug: videoSourceSize is %1x%2 for camera %3")).
        arg(m_videoSourceSize.width()).arg(m_videoSourceSize.height())
        .arg(getHostAddress()));

    bool trustToVideoSourceSize = false;
    for (const auto& resolution: m_primaryStreamCapabilities.resolutions)
    {
        if (resolution.width() <= m_videoSourceSize.width()
            && resolution.height() <= m_videoSourceSize.height())
        {
            // Trust to videoSourceSize if at least 1 appropriate resolution is exists.
            trustToVideoSourceSize = true;
            break;
        }

    }

    bool videoSourceSizeIsRight = resData.value<bool>(
        Qn::TRUST_TO_VIDEO_SOURCE_SIZE_PARAM_NAME, true);
    if (!videoSourceSizeIsRight)
        trustToVideoSourceSize = false;

    if (!trustToVideoSourceSize)
    {
        NX_DEBUG(this, QString(lit("ONVIF debug: do not trust to videoSourceSize is %1x%2 for camera %3 because it blocks all resolutions")).
            arg(m_videoSourceSize.width()).arg(m_videoSourceSize.height()).arg(getHostAddress()));
        return CameraDiagnostics::NoErrorResult();
    }

    QList<QSize>::iterator it = m_primaryStreamCapabilities.resolutions.begin();
    while (it != m_primaryStreamCapabilities.resolutions.end())
    {
        if (it->width() > m_videoSourceSize.width() || it->height() > m_videoSourceSize.height())
        {

            NX_DEBUG(this, QString(lit("ONVIF debug: drop resolution %1x%2 for camera %3 because resolution > videoSourceSize")).
                arg(it->width()).arg(it->width()).arg(getHostAddress()));

            it = m_primaryStreamCapabilities.resolutions.erase(it);
        }
        else
        {
            return CameraDiagnostics::NoErrorResult();
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper)
{
    AudioConfigsReq request;
    AudioConfigsResp response;

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoder: can't receive data from camera (or data is empty) (URL: "
            << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastErrorDescription();
#endif
        return false;

    }

    if (response.Configurations.empty())
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoder: empty data received from camera (or data is empty) (URL: "
            << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastErrorDescription();
#endif
        return false;
    }
    else
    {
        if ((int)response.Configurations.size() > getChannel())
        {
            onvifXsd__AudioEncoderConfiguration* conf = response.Configurations.at(getChannel());
        if (conf)
        {
            QnMutexLocker lock(&m_mutex);
            // TODO: #vasilenko UTF unuse std::string
            m_audioEncoderId = QString::fromStdString(conf->token);
        }
    }
#ifdef PL_ONVIF_DEBUG
        else
        {
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

    if (!source->Bounds)
    {
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
    MediaSoapWrapper soapWrapper(this);
    SetVideoSrcConfigReq request;
    SetVideoSrcConfigResp response;
    request.Configuration = source;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoSourceConfiguration(request, response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::setVideoSourceConfiguration: can't set required values into ONVIF physical device (URL: "
            << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastErrorDescription();
#endif

        if (soapWrapper.lastErrorIsNotAuthenticated())
            setStatus(Qn::Unauthorized);

        // Ignore error because of some cameras is not ONVIF profile S compatible
        // and doesn't support this request.
        return CameraDiagnostics::NoErrorResult();
        //return CameraDiagnostics::RequestFailedResult(QLatin1String("setVideoSourceConfiguration"), soapWrapper.getLastErrorDescription());
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchChannelCount(bool limitedByEncoders)
{
    MediaSoapWrapper soapWrapper(this);
    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);

    if (soapRes != SOAP_OK)
    {
        #ifdef PL_ONVIF_DEBUG
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: "
                "can't receive data from camera (or data is empty) (URL: "
                << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastErrorDescription();
        #endif
        if (soapWrapper.lastErrorIsNotAuthenticated())
            return CameraDiagnostics::NotAuthorisedResult(getMediaUrl());
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getVideoSources"), soapWrapper.getLastErrorDescription());

    }

    m_maxChannels = (int) response.VideoSources.size();

    if (m_maxChannels <= getChannel())
    {
        #ifdef PL_ONVIF_DEBUG
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: "
                << "empty data received from camera (or data is empty) (URL: "
                << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastErrorDescription();
        #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getVideoSources"),
            QLatin1String("missing video source configuration (1)"));
    }

    onvifXsd__VideoSource* conf = response.VideoSources.at(getChannel());

    if (!conf)
    {
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getVideoSources"),
            QLatin1String("missing video source configuration (2)"));
    }
    QnMutexLocker lock(&m_mutex);
    m_videoSourceToken = QString::fromStdString(conf->token);

    if (limitedByEncoders && m_maxChannels > 1)
    {
        VideoConfigsReq confRequest;
        VideoConfigsResp confResponse;
        // Get encoder list.
        soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse);
        if (soapRes != SOAP_OK)
        {
            return CameraDiagnostics::RequestFailedResult(
                QLatin1String("getVideoEncoderConfigurations"), soapWrapper.getLastErrorDescription());
        }
        int encoderCount = (int)confResponse.Configurations.size();

        //######################
        // we should also get encoder count via media2
        //######################

        if (encoderCount < m_maxChannels)
            m_maxChannels = encoderCount;
    }

    return CameraDiagnostics::NoErrorResult();
}

QRect QnPlOnvifResource::getVideoSourceMaxSize(const QString& configToken)
{
    MediaSoapWrapper soapWrapper(this);
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

    if (soapRes != SOAP_OK || !isValid)
    {
        #ifdef PL_ONVIF_DEBUG
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoSourceOptions: "
                << "can't receive data from camera (or data is empty) (URL: "
                << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastErrorDescription();
        #endif
        return QRect();
    }
    onvifXsd__IntRectangleRange* br = response.Options->BoundsRange;
    QRect result(qMax(0, br->XRange->Min), qMax(0, br->YRange->Min),
        br->WidthRange->Max, br->HeightRange->Max);
    if (result.isEmpty())
        return QRect();
    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoSource()
{
    CameraDiagnostics::Result result = fetchChannelCount();
    if (!result)
    {
        if (result.errorCode == CameraDiagnostics::ErrorCode::notAuthorised)
            setStatus(Qn::Unauthorized);
        return result;
    }

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    MediaSoapWrapper soapWrapper(this);
    VideoSrcConfigsReq request;
    VideoSrcConfigsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
        #ifdef PL_ONVIF_DEBUG
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: "
                << "can't receive data from camera (or data is empty) (URL: "
                << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastErrorDescription();
        #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getVideoSourceConfigurations"), soapWrapper.getLastErrorDescription());
    }

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    std::string srcToken = m_videoSourceToken.toStdString();
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        onvifXsd__VideoSourceConfiguration* conf = response.Configurations.at(i);
        if (!conf || conf->SourceToken != srcToken || !(conf->Bounds))
            continue;

        {
            QnMutexLocker lock(&m_mutex);
            m_videoSourceId = QString::fromStdString(conf->token);
        }

        QRect currentRect(
            conf->Bounds->x, conf->Bounds->y, conf->Bounds->width, conf->Bounds->height);
        QRect maxRect = getVideoSourceMaxSize(QString::fromStdString(conf->token));
        if (maxRect.isValid())
        {
            m_videoSourceSize = QSize(maxRect.width(), maxRect.height());

            //###################
            // seems to be wrong
            //###################
        }
        if (maxRect.isValid() && currentRect != maxRect && !isCameraControlDisabled())
        {
            updateVideoSource(conf, maxRect);
            return sendVideoSourceToCamera(conf);
        }
        else
        {
            return CameraDiagnostics::NoErrorResult();
        }

        if (commonModule()->isNeedToStop())
            return CameraDiagnostics::ServerTerminatedResult();
    }

    return CameraDiagnostics::UnknownErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetAudioSource()
{
    MediaSoapWrapper soapWrapper(this);
    AudioSrcConfigsReq request;
    AudioSrcConfigsResp response;

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
        #ifdef PL_ONVIF_DEBUG
            qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: "
                << "can't receive data from camera (or data is empty) (URL: "
                << soapWrapper.edpoint() << ", UniqueId: " << getUniqueId()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastErrorDescription();
        #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getAudioSourceConfigurations"), soapWrapper.getLastErrorDescription());
    }

    if ((int)response.Configurations.size() <= getChannel())
    {
        #ifdef PL_ONVIF_DEBUG
            qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: "
                << "empty data received from camera (or data is empty) (URL: "
                << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastErrorDescription();
        #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getAudioSourceConfigurations"),
            QLatin1String("missing channel configuration (1)"));
    }
    else
    {
        onvifXsd__AudioSourceConfiguration* conf = response.Configurations.at(getChannel());
        if (conf)
        {
            QnMutexLocker lock(&m_mutex);
            // TODO: #vasilenko UTF unuse std::string
            m_audioSourceId = QString::fromStdString(conf->token);
            return CameraDiagnostics::NoErrorResult();
        }
    }

    return CameraDiagnostics::RequestFailedResult(
        QLatin1String("getAudioSourceConfigurations"),
        QLatin1String("missing channel configuration (2)"));
}

bool QnPlOnvifResource::loadAdvancedParamsUnderLock(QnCameraAdvancedParamValueMap &values)
{
    m_prevOnvifResultCode = CameraDiagnostics::NoErrorResult();

    if (!m_imagingParamsProxy)
    {
        m_prevOnvifResultCode = CameraDiagnostics::UnknownErrorResult();
        return false;
    }

    m_prevOnvifResultCode = m_imagingParamsProxy->loadValues(values);
    return m_prevOnvifResultCode.errorCode == CameraDiagnostics::ErrorCode::noError;
}

std::vector<nx::mediaserver::resource::Camera::AdvancedParametersProvider*>
    QnPlOnvifResource::advancedParametersProviders()
{
    return {&m_advancedParametersProvider};
}

QnCameraAdvancedParamValueMap QnPlOnvifResource::getApiParameters(const QSet<QString>& ids)
{
    if (commonModule()->isNeedToStop())
        return {};

    QnMutexLocker lock(&m_physicalParamsMutex);
    m_advancedParamsCache.clear();
    if (loadAdvancedParamsUnderLock(m_advancedParamsCache))
    {
        m_advSettingsLastUpdated.restart();
    }
    else
    {
        m_advSettingsLastUpdated.invalidate();
        return {};
    }

    QnCameraAdvancedParamValueMap result;
    for(const QString &id: ids)
    {
        if (m_advancedParamsCache.contains(id))
            result.insert(id, m_advancedParamsCache[id]);
    }

    return result;
}

QSet<QString> QnPlOnvifResource::setApiParameters(const QnCameraAdvancedParamValueMap& values)
{
    QnCameraAdvancedParamValueList result;
    {
        QnMutexLocker lock(&m_physicalParamsMutex);
        setAdvancedParametersUnderLock(values.toValueList(), result);
        for (const auto& updatedValue: result)
            m_advancedParamsCache[updatedValue.id] = updatedValue.value;
    }

    QSet<QString> ids;
    for (const auto& updatedValue: result)
    {
        emit advancedParameterChanged(updatedValue.id, updatedValue.value);
        ids << updatedValue.id;
    }

    return ids;
}

bool QnPlOnvifResource::setAdvancedParameterUnderLock(
    const QnCameraAdvancedParameter &parameter, const QString &value)
{
    if (m_imagingParamsProxy && m_imagingParamsProxy->supportedParameters().contains(parameter.id))
        return m_imagingParamsProxy->setValue(parameter.id, value);

    if (m_maintenanceProxy && m_maintenanceProxy->supportedParameters().contains(parameter.id))
        return m_maintenanceProxy->callOperation(parameter.id);

    return false;
}

bool QnPlOnvifResource::setAdvancedParametersUnderLock(
    const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    bool success = true;
    for(const QnCameraAdvancedParamValue &value: values)
    {
        QnCameraAdvancedParameter parameter =
            m_advancedParametersProvider.getParameterById(value.id);
        if (parameter.isValid() && setAdvancedParameterUnderLock(parameter, value.value))
            result << value;
        else
            success = false;
    }
    return success;
}

SoapParams QnPlOnvifResource::makeSoapParams(
    OnvifWebService onvifWebService, bool tcpKeepAlive) const
{
    return SoapParams(onvifTimeouts(), m_serviceUrls.getUrl(onvifWebService).toStdString(),
        getAuth(), m_timeDrift, tcpKeepAlive);
}

SoapParams QnPlOnvifResource::makeSoapParams(
    std::string endpoint, bool tcpKeepAlive) const
{
    return SoapParams(onvifTimeouts(), std::move(endpoint),
        getAuth(), m_timeDrift, tcpKeepAlive);
}

/*
 * Positive number means timeout in seconds,
 * negative number - timeout in milliseconds.
 */
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

bool QnPlOnvifResource::loadAdvancedParametersTemplate(QnCameraAdvancedParams &params) const
{
    return loadXmlParametersInternal(params, lit(":/camera_advanced_params/onvif.xml"));
}

bool QnPlOnvifResource::loadXmlParametersInternal(
    QnCameraAdvancedParams &params, const QString& paramsTemplateFileName) const
{
    QFile paramsTemplateFile(paramsTemplateFileName);
#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif
    bool result = QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);

    if (!result)
    {
        NX_WARNING(this, lit("Error while parsing xml (onvif) %1")
            .arg(paramsTemplateFileName));
    }

    return result;
}

void QnPlOnvifResource::initAdvancedParametersProvidersUnderLock(QnCameraAdvancedParams &params)
{
    QAuthenticator auth = getAuth();
    QString imagingUrl = getImagingUrl();
    if (!imagingUrl.isEmpty())
    {
        m_imagingParamsProxy.reset(new QnOnvifImagingProxy(
            onvifTimeouts(),
            imagingUrl.toLatin1().data(),  auth.user(), auth.password(),
            m_videoSourceToken.toStdString(), m_timeDrift));
        m_imagingParamsProxy->initParameters(params);
    }

    QString maintenanceUrl = getDeviceOnvifUrl();
    if (!maintenanceUrl.isEmpty())
    {
        m_maintenanceProxy.reset(
            new QnOnvifMaintenanceProxy(onvifTimeouts(),
                maintenanceUrl, auth, m_videoSourceToken, m_timeDrift));
    }
}

QSet<QString> QnPlOnvifResource::calculateSupportedAdvancedParameters() const
{
    QSet<QString> result;
    if (m_imagingParamsProxy)
        result.unite(m_imagingParamsProxy->supportedParameters());
    if (m_maintenanceProxy)
        result.unite(m_maintenanceProxy->supportedParameters());
    return result;
}

void QnPlOnvifResource::fetchAndSetAdvancedParameters()
{
    QnMutexLocker lock(&m_physicalParamsMutex);

    m_advancedParametersProvider.clear();

    QnCameraAdvancedParams params;
    if (!loadAdvancedParametersTemplate(params))
        return;

    initAdvancedParametersProvidersUnderLock(params);

    QSet<QString> supportedParams = calculateSupportedAdvancedParameters();
    m_advancedParametersProvider.assign(params.filtered(supportedParams));
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoEncoderToCameraEx(
    onvifXsd__VideoEncoderConfiguration& encoder,
    Qn::StreamIndex /*streamIndex*/,
    const QnLiveStreamParams& /*params*/)
{
    return sendVideoEncoderToCamera(encoder);
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoEncoder2ToCameraEx(
    onvifXsd__VideoEncoder2Configuration& encoder,
    Qn::StreamIndex /*streamIndex*/,
    const QnLiveStreamParams& /*params*/)
{
    return sendVideoEncoder2ToCamera(encoder);
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoEncoderToCamera(onvifXsd__VideoEncoderConfiguration& encoderConfig)
{
    Media::VideoEncoderConfigurationSetter vecSetter(this);

    SetVideoConfigReq request;
    request.Configuration = &encoderConfig;
    request.ForcePersistence = false; //##########??? The ForcePersistence element is obsolete and should always be assumed to be true.
    vecSetter.performRequest(request);
    if (!vecSetter)
    {
        if (vecSetter.innerWrapper().lastErrorIsNotAuthenticated())
            setStatus(Qn::Unauthorized);
        // #TODO: Log.
        if (vecSetter.innerWrapper().getLastErrorDescription().contains("not possible to set"))
            return CameraDiagnostics::CannotConfigureMediaStreamResult("fps");   // TODO: #ak find param name
        else
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QString("'stream profile parameters'"));
    }
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoEncoder2ToCamera(onvifXsd__VideoEncoder2Configuration& encoderConfig)
{
    Media2::VideoEncoderConfigurationSetter vecSetter(this);
    Media2::VideoEncoderConfigurationSetter::Request request;
    request.Configuration = &encoderConfig;
    vecSetter.performRequest(request);
    if (!vecSetter)
    {
        if (vecSetter.innerWrapper().lastErrorIsNotAuthenticated())
            setStatus(Qn::Unauthorized);
        // #TODO: Log.
        if (vecSetter.innerWrapper().getLastErrorDescription().contains("not possible to set"))
            return CameraDiagnostics::CannotConfigureMediaStreamResult("fps");   // TODO: #ak find param name
        else
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QString("'stream profile parameters'"));
    }
    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::onRenewSubscriptionTimer(quint64 timerID)
{
    QnMutexLocker lk(&m_ioPortMutex);

    if (!m_eventCapabilities.get())
        return;
    if (timerID != m_renewSubscriptionTimerID)
        return;
    m_renewSubscriptionTimerID = 0;

    SubscriptionManagerSoapWrapper soapWrapper(this,
        m_onvifNotificationSubscriptionReference.isEmpty()
        ? m_eventCapabilities->XAddr
        : m_onvifNotificationSubscriptionReference.toStdString());
    soapWrapper.soap()->imode |= SOAP_XML_IGNORENS;

    char buf[256];

    _oasisWsnB2__Renew request;
    sprintf(buf, "PT%dS", DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT);
    std::string initialTerminationTime = buf;
    request.TerminationTime = &initialTerminationTime;

    // We need to construct an element shown below
    // <dom0:SubscriptionId xmlns:dom0="http://www.onvifplus.org/event">id</dom0:SubscriptionId>
    std::string id = m_onvifNotificationSubscriptionID.toStdString();
    soap_dom_attribute attribute(/*soap*/ nullptr, /*ns*/ nullptr,
        /*tag, i.e. name*/ "xmlns:dom0", /*text*/ "http://www.onvifplus.org/event");
    soap_dom_element element(/*soap*/ nullptr, /*ns*/ nullptr,
        /*tag, i.e. name*/ "dom0:SubscriptionId", /*text*/ id.c_str());
    element.atts = &attribute;

    if (!m_onvifNotificationSubscriptionID.isEmpty())
        request.__any.push_back(element);

    _oasisWsnB2__RenewResponse response;
    //NOTE: renewing session does not work on vista. Should ignore error in that case
    const int soapCallResult = soapWrapper.renew(request, response);
    if (soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND)
    {
        if (m_eventCapabilities->WSPullPointSupport)
        {
            /*
               According to PullPoint Notification mechanism 3 types of requests are used via
               onvifEvents::EventBindingProxy:
               CreatePullPointSubscription, PullMessages and Unsubscribe.
               Renew request is not supported and not needed.
               (https://www.onvif.org/specs/core/ONVIF-Core-Spec-v210.pdf
               section 9.2 Real-time Pull-Point Notification Interface).

               But in practice some cameras need Renew request, so we periodically send it via
               oasisWsnB2::SubscriptionManagerBindingProxy (EventBindingProxy doesn't have it).

               So it is ok, if camera doesn't recognize such a request.
               In case of SOAP_CLI_FAULT error we just stop Renew request sending.
            */
            NX_VERBOSE(this, lit("Ignoring renew error on %1").arg(getUrl()));
            if (soapCallResult == SOAP_CLI_FAULT)
                return;
        }
        else
        {
            /*
                In Basic Notification mechanism Renew request must be understood by camera.
                In case of error we unsubscribe, and try to subscribe again.
            */
            NX_DEBUG(this, lit("Failed to renew subscription (endpoint %1). %2").
                arg(QString::fromLatin1(soapWrapper.endpoint())).arg(soapCallResult));
            lk.unlock();

            _oasisWsnB2__Unsubscribe request;
            _oasisWsnB2__UnsubscribeResponse response;
            soapWrapper.unsubscribe(request, response);

            QnSoapServer::instance()->getService()->removeResourceRegistration(toSharedPointer(this));
            if (!registerNotificationConsumer())
            {
                lk.relock();
                scheduleRetrySubscriptionTimer(); //###### error???
            }
            return;
        }
    }
    else
    {
        NX_VERBOSE(this, lit("Renewed subscription to %1").arg(getUrl()));
    }

    unsigned int renewSubsciptionTimeoutSec = response.CurrentTime
        ? (response.TerminationTime - *response.CurrentTime)
        : DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;

    scheduleRenewSubscriptionTimer(renewSubsciptionTimeoutSec);
}

void QnPlOnvifResource::checkMaxFps(onvifXsd__VideoEncoderConfiguration* configuration)
{
    if (!configuration->RateControl)
        return;
    if (m_primaryStreamCapabilities.resolutions.isEmpty())
        return;

    // This code seems to be never executed. (???)
    int maxFpsOrig = getMaxFps();
    int rangeHi = getMaxFps()-2;
    int rangeLow = getMaxFps()/4;
    int currentFps = rangeHi;
    int prevFpsValue = -1;

    QSize resolution = m_primaryStreamCapabilities.resolutions[0];
    configuration->Resolution->Width = resolution.width();
    configuration->Resolution->Height = resolution.height();

    while (currentFps != prevFpsValue)
    {
        configuration->RateControl->FrameRateLimit = currentFps;
        bool success = false;
        bool invalidFpsDetected = false;
        for (int i = 0; i < getMaxOnvifRequestTries(); ++i)
        {
            if (commonModule()->isNeedToStop())
                return;

            configuration->RateControl->FrameRateLimit = currentFps;
            CameraDiagnostics::Result result = sendVideoEncoderToCamera(*configuration);
            if (result.errorCode == CameraDiagnostics::ErrorCode::noError)
            {
                if (currentFps >= maxFpsOrig-2)
                {
                    // If first try success, does not change maxFps at all. (HikVision has
                    // working range 0..15, and 25 fps, so try from max-1 checking).
                    return;
                }
                setMaxFps(currentFps);
                success = true;
                break;
            }
            else if (result.errorCode == CameraDiagnostics::ErrorCode::cannotConfigureMediaStream
                && result.errorParams.indexOf(QLatin1String("fps")) != -1)
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
        if (success)
        {
            rangeLow = currentFps;
            currentFps += (rangeHi-currentFps+1)/2;
        }
        else
        {
            rangeHi = currentFps-1;
            currentFps -= (currentFps-rangeLow+1)/2;
        }
    }
}

QnAbstractPtzController* QnPlOnvifResource::createSpecialPtzController() const
{
    if (getModel() == lit("DCS-5615"))
        return new QnDlinkPtzController(toSharedPointer(this));

    return 0;
}

QnAbstractPtzController *QnPlOnvifResource::createPtzControllerInternal() const
{
    QScopedPointer<QnAbstractPtzController> result;
    result.reset(createSpecialPtzController());
    if (result)
        return result.take();

    if (getPtzUrl().isEmpty() || getPtzConfigurationToken().isEmpty())
        return NULL;

    result.reset(new QnOnvifPtzController(toSharedPointer(this)));

    const auto operationalCapabilities
        = result->getCapabilities({nx::core::ptz::Type::operational});

    const auto configurationalCapabilities
        = result->getCapabilities({nx::core::ptz::Type::configurational});

    if (operationalCapabilities == Ptz::NoPtzCapabilities
        && configurationalCapabilities == Ptz::NoPtzCapabilities)
    {
        return NULL;
    }

    return result.take();
}

void QnPlOnvifResource::startInputPortStatesMonitoring()
{
    if (!m_eventCapabilities.get())
        return;

    {
        QnMutexLocker lk(&m_ioPortMutex);
        NX_ASSERT(!m_inputMonitored);
        m_inputMonitored = true;
    }

    const auto result = subscribeToCameraNotifications();
    NX_DEBUG(this, lit("Port monitoring has started: %1").arg(result));
}

void QnPlOnvifResource::scheduleRetrySubscriptionTimer()
{
    static const std::chrono::seconds kTimeout(
        DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT);

    NX_VERBOSE(this, lm("Schedule new subscription in %1").arg(kTimeout));
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
    if (m_eventCapabilities->WSPullPointSupport)
        return createPullPointSubscription();
    else if (QnSoapServer::instance()->initialized())
        return registerNotificationConsumer();
    else
        return false;
}

void QnPlOnvifResource::stopInputPortStatesMonitoring()
{
    //TODO #ak this method MUST become asynchronous
    quint64 nextPullMessagesTimerIDBak = 0;
    quint64 renewSubscriptionTimerIDBak = 0;
    {
        QnMutexLocker lk(&m_ioPortMutex);
        m_inputMonitored = false;
        nextPullMessagesTimerIDBak = m_nextPullMessagesTimerID;
        m_nextPullMessagesTimerID = 0;
        renewSubscriptionTimerIDBak = m_renewSubscriptionTimerID;
        m_renewSubscriptionTimerID = 0;
    }

    //removing timer
    if (nextPullMessagesTimerIDBak > 0)
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(nextPullMessagesTimerIDBak);
    if (renewSubscriptionTimerIDBak > 0)
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(renewSubscriptionTimerIDBak);
    //TODO #ak removing device event registration
        //if we do not remove event registration, camera will do it for us in some timeout

    QSharedPointer<GSoapAsyncPullMessagesCallWrapper> asyncPullMessagesCallWrapper;
    {
        QnMutexLocker lk(&m_ioPortMutex);
        std::swap(asyncPullMessagesCallWrapper, m_asyncPullMessagesCallWrapper);
    }

    if (asyncPullMessagesCallWrapper)
    {
        asyncPullMessagesCallWrapper->pleaseStop();
        asyncPullMessagesCallWrapper->join();
    }

    if (QnSoapServer::instance() && QnSoapServer::instance()->getService())
        QnSoapServer::instance()->getService()->removeResourceRegistration(toSharedPointer(this));

    NX_DEBUG(this, lit("Port monitoring is stopped"));
}

//////////////////////////////////////////////////////////
// QnPlOnvifResource::SubscriptionReferenceParametersParseHandler
//////////////////////////////////////////////////////////

QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::SubscriptionReferenceParametersParseHandler()
:
    m_readingSubscriptionID(false)
{
}

bool QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::characters(const QString& ch)
{
    if (m_readingSubscriptionID)
        subscriptionID = ch;
    return true;
}

bool QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::startElement(
    const QString& /*namespaceURI*/,
    const QString& localName,
    const QString& /*qName*/,
    const QXmlAttributes& /*atts*/)
{
    if (localName == QLatin1String("SubscriptionId"))
        m_readingSubscriptionID = true;
    return true;
}

bool QnPlOnvifResource::SubscriptionReferenceParametersParseHandler::endElement(
    const QString& /*namespaceURI*/,
    const QString& localName,
    const QString& /*qName*/)
{
    if (localName == QLatin1String("SubscriptionId"))
        m_readingSubscriptionID = false;
    return true;
}

//////////////////////////////////////////////////////////
// QnPlOnvifResource
//////////////////////////////////////////////////////////

bool QnPlOnvifResource::createPullPointSubscription()
{
    EventSoapWrapper soapWrapper(this, m_eventCapabilities->XAddr);
    soapWrapper.soap()->imode |= SOAP_XML_IGNORENS;

    _onvifEvents__CreatePullPointSubscription request;
    std::string initialTerminationTime = "PT600S";
    request.InitialTerminationTime = &initialTerminationTime;
    _onvifEvents__CreatePullPointSubscriptionResponse response;
    const int soapCallResult = soapWrapper.createPullPointSubscription(request, response);
    if (soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND)
    {
        NX_WARNING(this, lm("Failed to subscribe to %1").arg(soapWrapper.endpoint()));
        scheduleRenewSubscriptionTimer(RENEW_NOTIFICATION_FORWARDING_SECS);
        return false;
    }

    NX_VERBOSE(this, lm("Successfully created pool point to %1").arg(soapWrapper.endpoint()));
    if (response.SubscriptionReference.ReferenceParameters &&
        response.SubscriptionReference.ReferenceParameters->__any)
    {
        // Parsing to retrieve subscriptionId. The example of parsed string (subscriptionId is 3):
        // <dom0:SubscriptionId xmlns:dom0="http://www.cellinx.co.kr/event">3</dom0:SubscriptionId>
        QXmlSimpleReader reader;
        SubscriptionReferenceParametersParseHandler handler;
        reader.setContentHandler(&handler);
        QBuffer srcDataBuffer;
        srcDataBuffer.setData(
            response.SubscriptionReference.ReferenceParameters->__any[0],
            (int)strlen(response.SubscriptionReference.ReferenceParameters->__any[0]));
        QXmlInputSource xmlSrc(&srcDataBuffer);
        if (reader.parse(&xmlSrc))
            m_onvifNotificationSubscriptionID = handler.subscriptionID;
    }

    if (response.SubscriptionReference.Address)
    {
        m_onvifNotificationSubscriptionReference =
            fromOnvifDiscoveredUrl(response.SubscriptionReference.Address);
    }

    QnMutexLocker lk(&m_ioPortMutex);

    if (!m_inputMonitored)
        return true;

    if (resourceData().value<bool>(lit("renewOnvifPullPointSubscriptionRequired"), true))
    {
        // Adding task to refresh subscription (does not work on Vista cameras).
        // TODO: detailed explanation about SubscriptionManagerBindingProxy::Renew.
        const unsigned int renewSubsciptionTimeoutSec =
            response.oasisWsnB2__TerminationTime - response.oasisWsnB2__CurrentTime;
        scheduleRenewSubscriptionTimer(renewSubsciptionTimeoutSec);
    }

    m_eventMonitorType = emtPullPoint;
    m_prevPullMessageResponseClock = m_monotonicClock.elapsed();

    updateTimer(&m_nextPullMessagesTimerID,
        std::chrono::milliseconds(PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC * MS_PER_SECOND),
        std::bind(&QnPlOnvifResource::pullMessages, this, std::placeholders::_1));
    return true;
}

void QnPlOnvifResource::removePullPointSubscription()
{
    SubscriptionManagerSoapWrapper soapWrapper(this,
        !m_onvifNotificationSubscriptionReference.isEmpty()
        ? m_onvifNotificationSubscriptionReference.toStdString()
        : m_eventCapabilities->XAddr /*emergency option*/);
    soapWrapper.soap()->imode |= SOAP_XML_IGNORENS;

    _oasisWsnB2__Unsubscribe request;

    // We need to construct an element shown below
    // <dom0:SubscriptionId xmlns:dom0="http://www.onvifplus.org/event">id</dom0:SubscriptionId>
    std::string id = m_onvifNotificationSubscriptionID.toStdString();
    soap_dom_attribute attribute(/*soap*/ nullptr, /*ns*/ nullptr,
        /*tag, i.e. name*/ "xmlns:dom0", /*text*/ "http://www.onvifplus.org/event");
    soap_dom_element element(/*soap*/ nullptr, /*ns*/ nullptr,
        /*tag, i.e. name*/ "dom0:SubscriptionId", /*text*/ id.c_str());
    element.atts = &attribute;

    if (!m_onvifNotificationSubscriptionID.isEmpty())
        request.__any.push_back(element);

    _oasisWsnB2__UnsubscribeResponse response;
    const int soapCallResult = soapWrapper.unsubscribe(request, response);
    if (soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND)
    {
        NX_DEBUG(this, lit("Failed to unsubscribe from %1, result code %2").
            arg(QString::fromLatin1(soapWrapper.endpoint())).arg(soapCallResult));
        return;
    }
}

template<class _NumericInt>
_NumericInt roundUp(_NumericInt val, _NumericInt step, typename std::enable_if<std::is_integral<_NumericInt>::value>::type* = nullptr)
{
    if (step == 0)
        return val;
    return (val + step - 1) / step * step;
}

void QnPlOnvifResource::pullMessages(quint64 timerID)
{
    static const int MAX_MESSAGES_TO_PULL = 10;

    QnMutexLocker lk(&m_ioPortMutex);

    if (timerID != m_nextPullMessagesTimerID)
    {
        // This can actually happen, if we call
        // startInputPortMonitoring, stopInputPortMonitoring, startInputPortMonitoring too quick.
        // TODO: log.
        return;
    }
    m_nextPullMessagesTimerID = 0;

    if (!m_inputMonitored)
        return;

    if (m_asyncPullMessagesCallWrapper)
    {
        // Previous request is still running, new timer will be added within completion handler.
        // TODO: log.
        return;
    }
    QAuthenticator auth = getAuth();

    std::unique_ptr<PullPointSubscriptionWrapper> soapWrapper(
        new PullPointSubscriptionWrapper(
            onvifTimeouts(),
            !m_onvifNotificationSubscriptionReference.isEmpty()
                ? m_onvifNotificationSubscriptionReference.toStdString()
                : m_eventCapabilities->XAddr, //< emergency option
            auth.user(),
            auth.password(),
            m_timeDrift));
    soapWrapper->soap()->imode |= SOAP_XML_IGNORENS;

    std::vector<void*> memToFreeOnResponseDone;
    memToFreeOnResponseDone.reserve(3); //we have 3 memory allocation below

    char* buf = (char*)malloc(512);
    memToFreeOnResponseDone.push_back(buf);

    _onvifEvents__PullMessages request;
    request.Timeout = (m_monotonicClock.elapsed() - m_prevPullMessageResponseClock) / 1000 * 1000;
    request.MessageLimit = MAX_MESSAGES_TO_PULL;
    QByteArray onvifNotificationSubscriptionIDLatin1 = m_onvifNotificationSubscriptionID.toLatin1();
    strcpy(buf, onvifNotificationSubscriptionIDLatin1.data());
    struct SOAP_ENV__Header* header = (struct SOAP_ENV__Header*)malloc(sizeof(SOAP_ENV__Header));
    memToFreeOnResponseDone.push_back(header);
    memset(header, 0, sizeof(*header));
    soapWrapper->soap()->header = header;
    soapWrapper->soap()->header->subscriptionID = buf;
    //TODO #ak move away check for "Samsung"
    if (!m_onvifNotificationSubscriptionReference.isEmpty() && !getVendor().contains(lit("Samsung")))
    {
        const QByteArray& onvifNotificationSubscriptionReferenceUtf8 = m_onvifNotificationSubscriptionReference.toUtf8();
        char* buf = (char*)malloc(onvifNotificationSubscriptionReferenceUtf8.size()+1);
        memToFreeOnResponseDone.push_back(buf);
        strcpy(buf, onvifNotificationSubscriptionReferenceUtf8.constData());
        soapWrapper->soap()->header->wsa5__To = buf;
    }

    // Very few devices need wsa__Action and wsa__ReplyTo to be filled, but sometimes they do.
    wsa5__EndpointReferenceType* replyTo =
        (wsa5__EndpointReferenceType*)malloc(sizeof(wsa5__EndpointReferenceType));
    memToFreeOnResponseDone.push_back(replyTo);
    memset(replyTo, 0, sizeof(*replyTo));
    static const char* kReplyTo = "http://www.w3.org/2005/08/addressing/anonymous";
    replyTo->Address = const_cast<char*>(kReplyTo);
    header->wsa5__ReplyTo = replyTo;

    static const char* kAction =
        "http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest";
    header->wsa5__Action = const_cast<char*>(kAction);

    _onvifEvents__PullMessagesResponse response;

    auto resData = resourceData();
    const bool useHttpReader = resData.value<bool>(
        Qn::PARSE_ONVIF_NOTIFICATIONS_WITH_HTTP_READER,
        false);

    QSharedPointer<GSoapAsyncPullMessagesCallWrapper> asyncPullMessagesCallWrapper(
        new GSoapAsyncPullMessagesCallWrapper(
            std::move(soapWrapper),
            &PullPointSubscriptionWrapper::pullMessages,
            useHttpReader),
        [memToFreeOnResponseDone](GSoapAsyncPullMessagesCallWrapper* ptr)
        {
            for(void* pObj: memToFreeOnResponseDone)
                ::free(pObj);
            delete ptr;
        });

    NX_VERBOSE(this, lm("Pull messages from %1 with httpReader=%2").args(
        m_onvifNotificationSubscriptionReference, useHttpReader));

    using namespace std::placeholders;
    asyncPullMessagesCallWrapper->callAsync(
        request,
        std::bind(
            &QnPlOnvifResource::onPullMessagesDone, this, asyncPullMessagesCallWrapper.data(), _1));

    m_asyncPullMessagesCallWrapper = std::move(asyncPullMessagesCallWrapper);
}

void QnPlOnvifResource::onPullMessagesDone(
    GSoapAsyncPullMessagesCallWrapper* asyncWrapper, int resultCode)
{
    using namespace std::placeholders;

    auto SCOPED_GUARD_FUNC = [this](QnPlOnvifResource*){
        m_asyncPullMessagesCallWrapper.clear();
    };
    std::unique_ptr<QnPlOnvifResource, decltype(SCOPED_GUARD_FUNC)>
        SCOPED_GUARD(this, SCOPED_GUARD_FUNC);

    if ((resultCode != SOAP_OK && resultCode != SOAP_MUSTUNDERSTAND) ||  //error has been reported by camera
        (asyncWrapper->response().soap &&
            asyncWrapper->response().soap->header &&
            asyncWrapper->response().soap->header->wsa5__Action &&
            strstr(asyncWrapper->response().soap->header->wsa5__Action, "/soap/fault") != nullptr))
    {
        NX_DEBUG(this, lit("Failed to pull messages from %1, result code %2").
            arg(QString::fromLatin1(asyncWrapper->syncWrapper()->endpoint())).
            arg(resultCode));
        //re-subscribing

        QnMutexLocker lk(&m_ioPortMutex);

        if (!m_inputMonitored)
            return;

        return updateTimer(&m_renewSubscriptionTimerID, std::chrono::milliseconds::zero(),
            std::bind(&QnPlOnvifResource::renewPullPointSubscriptionFallback, this, _1));
    }

    onPullMessagesResponseReceived(asyncWrapper->syncWrapper(), resultCode, asyncWrapper->response());

    QnMutexLocker lk(&m_ioPortMutex);

    if (!m_inputMonitored)
        return;

    using namespace std::placeholders;
    NX_ASSERT(m_nextPullMessagesTimerID == 0);
    if (m_nextPullMessagesTimerID == 0)    //otherwise, we already have timer somehow
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
    NX_ASSERT(resultCode == SOAP_OK || resultCode == SOAP_MUSTUNDERSTAND);

    const qint64 currentRequestSendClock = m_monotonicClock.elapsed();

    const time_t minNotificationTime = response.CurrentTime - roundUp<qint64>(m_monotonicClock.elapsed() - m_prevPullMessageResponseClock, MS_PER_SECOND) / MS_PER_SECOND;
    if (response.oasisWsnB2__NotificationMessage.size() > 0)
    {
        for(size_t i = 0; i < response.oasisWsnB2__NotificationMessage.size(); ++i)
        {
            notificationReceived(
                *response.oasisWsnB2__NotificationMessage[i], minNotificationTime);
        }
    }

    m_prevPullMessageResponseClock = currentRequestSendClock;
}

bool QnPlOnvifResource::fetchRelayOutputs(std::vector<RelayOutputInfo>* relayOutputInfoList)
{
    DeviceIO::RelayOutputs relayOutputs(this);
    relayOutputs.receiveBySoap();

    if (!relayOutputs && relayOutputs.soapError() != SOAP_MUSTUNDERSTAND)
    {
        NX_DEBUG(this, lit("Failed to get relay input/output info. endpoint %1").arg(relayOutputs.endpoint()));
        return false;
    }
    auto data = relayOutputs.get();

    m_relayOutputInfo.clear();
    if (data->RelayOutputs.size() > MAX_IO_PORTS_PER_DEVICE)
    {
        NX_DEBUG(this, lit("Device has too many relay outputs. endpoint %1")
            .arg(relayOutputs.endpoint()));
        return false;
    }

    for(size_t i = 0; i < data->RelayOutputs.size(); ++i)
    {
        m_relayOutputInfo.push_back(RelayOutputInfo(
            data->RelayOutputs[i]->token,
            data->RelayOutputs[i]->Properties->Mode == onvifXsd__RelayMode::Bistable,
            data->RelayOutputs[i]->Properties->DelayTime,
            data->RelayOutputs[i]->Properties->IdleState == onvifXsd__RelayIdleState::closed));
    }

    if (relayOutputInfoList)
        *relayOutputInfoList = m_relayOutputInfo;

    NX_DEBUG(this, lit("Successfully got device (%1) output ports info. Found %2 relay output").
        arg(relayOutputs.endpoint()).arg(m_relayOutputInfo.size()));

    return true;
}

bool QnPlOnvifResource::fetchRelayOutputInfo(const std::string& outputID, RelayOutputInfo* const relayOutputInfo)
{
    fetchRelayOutputs(NULL);
    for(std::vector<RelayOutputInfo>::size_type
         i = 0;
         i < m_relayOutputInfo.size();
        ++i)
    {
        if (m_relayOutputInfo[i].token == outputID || outputID.empty())
        {
            *relayOutputInfo = m_relayOutputInfo[i];
            return true;
        }
    }

    return false; //there is no output with id outputID
}

bool QnPlOnvifResource::setRelayOutputInfo(const RelayOutputInfo& relayOutputInfo)
{
    QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift);

    NX_DEBUG(this, lit("Swiching camera %1 relay output %2 to monostable mode").
        arg(QString::fromLatin1(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)));

    //switching to monostable mode
    _onvifDevice__SetRelayOutputSettings setOutputSettingsRequest;
    setOutputSettingsRequest.RelayOutputToken = relayOutputInfo.token;
    onvifXsd__RelayOutputSettings relayOutputSettings;
    relayOutputSettings.Mode = relayOutputInfo.isBistable ? onvifXsd__RelayMode::Bistable : onvifXsd__RelayMode::Monostable;

    relayOutputSettings.DelayTime = relayOutputInfo.delayTimeMs;

    relayOutputSettings.IdleState = relayOutputInfo.activeByDefault ? onvifXsd__RelayIdleState::closed : onvifXsd__RelayIdleState::open;
    setOutputSettingsRequest.Properties = &relayOutputSettings;
    _onvifDevice__SetRelayOutputSettingsResponse setOutputSettingsResponse;
    const int soapCallResult = soapWrapper.setRelayOutputSettings(setOutputSettingsRequest, setOutputSettingsResponse);
    if (soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND)
    {
        NX_WARNING(this, lit("Failed to switch camera %1 relay output %2 to monostable mode. %3").
            arg(QString::fromLatin1(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)).arg(soapCallResult));
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
    {
        QnMutexLocker lock(&m_layoutMutex);
        if (m_videoLayout)
            return m_videoLayout;
    }

    auto resData = resourceData();
    auto layoutStr = resData.value<QString>(Qn::VIDEO_LAYOUT_PARAM_NAME2);
    auto videoLayout = layoutStr.isEmpty()
        ? QnMediaResource::getVideoLayout(dataProvider)
        : QnConstResourceVideoLayoutPtr(QnCustomResourceVideoLayout::fromString(layoutStr));

    auto nonConstThis = const_cast<QnPlOnvifResource*>(this);
    {
        QnMutexLocker lock(&m_layoutMutex);
        m_videoLayout = videoLayout;
        nonConstThis->setProperty(Qn::VIDEO_LAYOUT_PARAM_NAME, videoLayout->toString());
        nonConstThis->saveParams();
    }

    return m_videoLayout;
}

void QnPlOnvifResource::setOutputPortStateNonSafe(
    quint64 timerID,
    const QString& outputID,
    bool active,
    unsigned int autoResetTimeoutMS)
{
    {
        QnMutexLocker lk(&m_ioPortMutex);
        m_triggerOutputTasks.erase(timerID);
    }

    //retrieving output info to check mode
    RelayOutputInfo relayOutputInfo;
    if (!fetchRelayOutputInfo(outputID.toStdString(), &relayOutputInfo))
    {
        NX_WARNING(this, lit("Cannot change relay output %1 state. Failed to get relay output info").arg(outputID));
        return /*false*/;
    }

#ifdef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    const bool isBistableModeRequired = true;
#else
    const bool isBistableModeRequired = autoResetTimeoutMS == 0;
#endif

#ifndef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    std::string requiredDelayTime;
    if (autoResetTimeoutMS > 0)
    {
        std::ostringstream ss;
        ss<<"PT"<<(autoResetTimeoutMS < 1000 ? 1 : autoResetTimeoutMS/1000)<<"S";
        requiredDelayTime = ss.str();
    }
#endif
    if ((relayOutputInfo.isBistable != isBistableModeRequired) ||
#ifndef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
        (!isBistableModeRequired && relayOutputInfo.delayTimeMs != requiredDelayTime) ||
#endif
        relayOutputInfo.activeByDefault)
    {
        //switching output to required mode
        relayOutputInfo.isBistable = isBistableModeRequired;
#ifndef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
        relayOutputInfo.delayTimeMs = requiredDelayTime;
#endif
        relayOutputInfo.activeByDefault = false;
        if (!setRelayOutputInfo(relayOutputInfo))
        {
            NX_WARNING(this, lit("Cannot set camera %1 output %2 to state %3 with timeout %4 msec. Cannot set mode to %5").
                arg(QString()).arg(QString::fromStdString(relayOutputInfo.token)).arg(QLatin1String(active ? "active" : "inactive")).arg(autoResetTimeoutMS).
                arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")));
            return /*false*/;
        }

        NX_WARNING(this, lit("Camera %1 output %2 has been switched to %3 mode").arg(QString()).arg(outputID).
            arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")));
    }

    //modifying output
    QAuthenticator auth = getAuth();

    DeviceSoapWrapper soapWrapper(
        onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift);

    _onvifDevice__SetRelayOutputState request;
    request.RelayOutputToken = relayOutputInfo.token;

    const auto onvifActive = m_isRelayOutputInversed ? !active : active;
    request.LogicalState = onvifActive ? onvifXsd__RelayLogicalState::active : onvifXsd__RelayLogicalState::inactive;

    _onvifDevice__SetRelayOutputStateResponse response;
    const int soapCallResult = soapWrapper.setRelayOutputState(request, response);
    if (soapCallResult != SOAP_OK && soapCallResult != SOAP_MUSTUNDERSTAND)
    {
        NX_WARNING(this, lm("Failed to set relay %1 output state to %2. endpoint %3")
            .args(relayOutputInfo.token, onvifActive, soapWrapper.endpoint()));
        return /*false*/;
    }

#ifdef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    if ((autoResetTimeoutMS > 0) && active)
    {
        //adding task to reset port state
        using namespace std::placeholders;
        const quint64 timerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnPlOnvifResource::setOutputPortStateNonSafe, this, _1, outputID, !active, 0),
            std::chrono::milliseconds(autoResetTimeoutMS));
        m_triggerOutputTasks[timerID] = TriggerOutputTask(outputID, !active, 0);
    }
#endif

    NX_INFO(this, lm("Successfully set relay %1 output state to %2. endpoint %3")
        .args(relayOutputInfo.token, onvifActive, soapWrapper.endpoint()));
    return /*true*/;
}

QnMutex* QnPlOnvifResource::getStreamConfMutex()
{
    return &m_streamConfMutex;
}

void QnPlOnvifResource::beforeConfigureStream(Qn::ConnectionRole /*role*/)
{
    QnMutexLocker lock(&m_streamConfMutex);
    ++m_streamConfCounter;
    while (m_streamConfCounter > 1)
        m_streamConfCond.wait(&m_streamConfMutex);
}

void QnPlOnvifResource::afterConfigureStream(Qn::ConnectionRole /*role*/)
{
    QnMutexLocker lock(&m_streamConfMutex);
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
    auto resData = resourceData();
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
    const QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

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
    const QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);
    CapabilitiesResp response;
    auto result = fetchOnvifCapabilities(soapWrapper, &response);
    if (!result)
        return result;

    if (response.Capabilities)
        fillFullUrlInfo(response);

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
    DeviceSoapWrapper& soapWrapper,
    CapabilitiesResp* response)
{
    //Trying to get onvif URLs
    CapabilitiesReq request;
    int soapRes = soapWrapper.getCapabilities(request, *response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
            << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastErrorDescription();
#endif
        if (soapWrapper.lastErrorIsNotAuthenticated())
        {
            if (!getId().isNull())
                setStatus(Qn::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult(getDeviceOnvifUrl());
        }
        return CameraDiagnostics::RequestFailedResult(lit("getCapabilities"), soapWrapper.getLastErrorDescription());
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchOnvifMedia2Url(QString* url)
{
    const QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    GetServicesReq request;
    GetServicesResp response;
    int soapRes = soapWrapper.getServices(request, response);
    if (soapRes != SOAP_OK)
    {

#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
            << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastErrorDescription();
#endif
        return CameraDiagnostics::RequestFailedResult("getCapabilities", soapWrapper.getLastErrorDescription());
    }

    for (const onvifDevice__Service* service: response.Service)
    {
        if (service && service->Namespace == kOnvifMedia2Namespace)
        {
            *url = fromOnvifDiscoveredUrl(service->XAddr);
            break;
        }
    }
    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::fillFullUrlInfo(const CapabilitiesResp& response)
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
        setDeviceIOUrl(fromOnvifDiscoveredUrl(response.Capabilities->Extension->DeviceIO->XAddr));
    }
    else
    {
        setDeviceIOUrl(getDeviceOnvifUrl());
    }
}

/**
 * Some cameras provide model in a bit different way via native driver and ONVIF driver.
 * Try several variants to match json data.
 */
bool QnPlOnvifResource::isCameraForcedToOnvif(
    QnResourceDataPool* dataPool,
    const QString& manufacturer, const QString& model)
{
    QnResourceData resourceData = dataPool->data(manufacturer, model);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return true;

    QString shortModel = model;
    shortModel.replace(QString(lit(" ")), QString());
    shortModel.replace(QString(lit("-")), QString());
    resourceData = dataPool->data(manufacturer, shortModel);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return true;

    if (shortModel.startsWith(manufacturer))
        shortModel = shortModel.mid(manufacturer.length()).trimmed();
    resourceData = dataPool->data(manufacturer, shortModel);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return true;

    return false;
}

QnAudioTransmitterPtr QnPlOnvifResource::initializeTwoWayAudio()
{
    if (auto result = initializeTwoWayAudioByResourceData())
        return result;

    MediaSoapWrapper soapWrapper(this);

    //TODO: consider to move it to streamReader and change it to GetCompatibleAudioOutputConfigurations
    GetAudioOutputConfigurationsReq request;
    GetAudioOutputConfigurationsResp response;
    const int result = soapWrapper.getAudioOutputConfigurations(request, response);
    if (result != SOAP_OK && result != SOAP_MUSTUNDERSTAND)
    {
        NX_VERBOSE(this, lm("Filed to fetch audio outputs from %1").arg(soapWrapper.endpoint()));
        return QnAudioTransmitterPtr();
    }

    if (!response.Configurations.empty())
    {
        setAudioOutputConfigurationToken(QString::fromStdString(
            response.Configurations.front()->token));
        NX_VERBOSE(this, lm("Detected audio output %1 on %2").args(
            audioOutputConfigurationToken(), soapWrapper.endpoint()));

        return std::make_shared<nx::mediaserver_core::plugins::OnvifAudioTransmitter>(this);
    }

    NX_VERBOSE(this, lm("No sutable audio outputs are detected on %1").arg(soapWrapper.endpoint()));
    return QnAudioTransmitterPtr();
}

QString QnPlOnvifResource::audioOutputConfigurationToken() const
{
    QnMutexLocker lock(&m_mutex);
    return m_audioOutputConfigurationToken;
}

void QnPlOnvifResource::setAudioOutputConfigurationToken(const QString& value)
{
    QnMutexLocker lock(&m_mutex);
    m_audioOutputConfigurationToken = value;
}

QnAudioTransmitterPtr QnPlOnvifResource::initializeTwoWayAudioByResourceData()
{
    QnAudioTransmitterPtr result;
    TwoWayAudioParams params = resourceData().value<TwoWayAudioParams>(Qn::TWO_WAY_AUDIO_PARAM_NAME);
    if (params.engine.toLower() == QString("onvif"))
    {
        result.reset(new nx::mediaserver_core::plugins::OnvifAudioTransmitter(this));
    }
    else if (params.engine.toLower() == QString("basic") || params.engine.isEmpty())
    {
        if (params.codec.isEmpty() || params.urlPath.isEmpty())
            return result;

        auto audioTransmitter = std::make_unique<QnBasicAudioTransmitter>(this);
        audioTransmitter->setContentType(params.contentType.toUtf8());

        if (params.noAuth)
            audioTransmitter->setAuthPolicy(QnBasicAudioTransmitter::AuthPolicy::noAuth);
        else if (params.useBasicAuth)
            audioTransmitter->setAuthPolicy(QnBasicAudioTransmitter::AuthPolicy::basicAuth);
        else
            audioTransmitter->setAuthPolicy(QnBasicAudioTransmitter::AuthPolicy::digestAndBasicAuth);

        nx::utils::Url srcUrl(getUrl());
        nx::utils::Url url(lit("http://%1:%2%3")
            .arg(srcUrl.host())
            .arg(srcUrl.port(nx::network::http::DEFAULT_HTTP_PORT))
            .arg(params.urlPath));
        audioTransmitter->setTransmissionUrl(url);

        result.reset(audioTransmitter.release());
    }
    else
    {
        NX_ASSERT(false, lm("Unsupported 2WayAudio engine: %1").arg(params.engine));
        return result;
    }

    if (!params.codec.isEmpty())
    {
        QnAudioFormat format;
        format.setCodec(params.codec);
        format.setSampleRate(params.sampleRate * 1000);
        format.setChannelCount(params.channels);
        result->setOutputFormat(format);
    }

    if (params.bitrateKbps != 0)
        result->setBitrateKbps(params.bitrateKbps * 1000);

    return result;
}

void QnPlOnvifResource::setMaxChannels(int value)
{
    m_maxChannels = value;
}

void QnPlOnvifResource::updateVideoEncoder(
    onvifXsd__VideoEncoderConfiguration& encoder,
    Qn::StreamIndex streamIndex,
    const QnLiveStreamParams& streamParams)
{
    QnLiveStreamParams params = streamParams;
    const auto resourceData = this->resourceData();

    bool useEncodingInterval = resourceData.value<bool>
        (Qn::CONTROL_FPS_VIA_ENCODING_INTERVAL_PARAM_NAME);

    if (getProperty(QnMediaResource::customAspectRatioKey()).isEmpty())
    {
        bool forcedAR = resourceData.value<bool>(QString("forceArFromPrimaryStream"), false);
        if (forcedAR && params.resolution.height() > 0)
        {
            QnAspectRatio ar(params.resolution.width(), params.resolution.height());
            setCustomAspectRatio(ar);
        }

        QString defaultAR = resourceData.value<QString>(QString("defaultAR"));
        QStringList parts = defaultAR.split(L'x');
        if (parts.size() == 2)
        {
            QnAspectRatio ar(parts[0].toInt(), parts[1].toInt());
            setCustomAspectRatio(ar);
        }
        saveParams();
    }
    const auto capabilities = (streamIndex == Qn::StreamIndex::primary)
        ? m_primaryStreamCapabilities : m_secondaryStreamCapabilities;

    const auto codecId = QnAvCodecHelper::codecIdFromString(streamParams.codec);
    switch (codecId)
    {
        case AV_CODEC_ID_MJPEG:
            encoder.Encoding = onvifXsd__VideoEncoding::JPEG;
            break;
        case AV_CODEC_ID_H264:
            encoder.Encoding = onvifXsd__VideoEncoding::H264;
            break;
        default:
            // ONVIF Media1 interface doesn't support other codecs
            // (except MPEG4, but VMS doesn't support it), so we use JPEG in this case,
            // because it is usually supported by all devices
            encoder.Encoding = onvifXsd__VideoEncoding::JPEG;
    }

    if (encoder.Encoding == onvifXsd__VideoEncoding::H264)
    {
        if (encoder.H264 == 0)
            encoder.H264 = m_tmpH264Conf.get();

        encoder.H264->GovLength = qBound(capabilities.govMin, DEFAULT_IFRAME_DISTANCE, capabilities.govMax);
        auto h264Profile = getH264StreamProfile(capabilities);
        if (h264Profile.is_initialized())
            encoder.H264->H264Profile = *h264Profile;
        if (encoder.RateControl)
            encoder.RateControl->EncodingInterval = 1;
    }

    if (!encoder.RateControl)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: RateControl is NULL. UniqueId: " << m_onvifRes->getUniqueId();
#endif
    }
    else
    {
        if (!useEncodingInterval)
        {
            encoder.RateControl->FrameRateLimit = params.fps;
        }
        else
        {
            int fpsBase = resourceData.value<int>(Qn::FPS_BASE_PARAM_NAME);
            params.fps = getClosestAvailableFps(params.fps);
            encoder.RateControl->FrameRateLimit = fpsBase;
            encoder.RateControl->EncodingInterval = static_cast<int>(
                fpsBase / params.fps + 0.5);
        }

        encoder.RateControl->BitrateLimit = params.bitrateKbps;
    }

    const Qn::StreamQuality quality = params.quality;
    if (quality != Qn::StreamQuality::preset)
        encoder.Quality = innerQualityToOnvif(quality, capabilities.minQ, capabilities.maxQ);

    if (!encoder.Resolution)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: Resolution is NULL. UniqueId: " << m_onvifRes->getUniqueId();
#endif
    }
    else
    {
        encoder.Resolution->Width = params.resolution.width();
        encoder.Resolution->Height = params.resolution.height();
    }
}

void QnPlOnvifResource::updateVideoEncoder2(
    onvifXsd__VideoEncoder2Configuration& encoder,
    Qn::StreamIndex streamIndex,
    const QnLiveStreamParams& streamParams)
{
    QnLiveStreamParams params = streamParams;
    const QnResourceData resourceData = this->resourceData();

    bool useEncodingInterval = resourceData.value<bool>
        (Qn::CONTROL_FPS_VIA_ENCODING_INTERVAL_PARAM_NAME);

    if (getProperty(QnMediaResource::customAspectRatioKey()).isEmpty())
    {
        bool forcedAR = resourceData.value<bool>(QString("forceArFromPrimaryStream"), false);
        if (forcedAR && params.resolution.height() > 0)
        {
            QnAspectRatio ar(params.resolution.width(), params.resolution.height());
            setCustomAspectRatio(ar);
        }

        QString defaultAR = resourceData.value<QString>(QString("defaultAR"));
        QStringList parts = defaultAR.split(L'x');
        if (parts.size() == 2)
        {
            QnAspectRatio ar(parts[0].toInt(), parts[1].toInt());
            setCustomAspectRatio(ar);
        }
        saveParams();
    }
    const auto capabilities = (streamIndex == Qn::StreamIndex::primary)
        ? m_primaryStreamCapabilities : m_secondaryStreamCapabilities;

    const AVCodecID codecId = QnAvCodecHelper::codecIdFromString(streamParams.codec);
    static const std::map<AVCodecID,std::string> kMedia2EncodingMap =
        { {AV_CODEC_ID_MJPEG, "JPEG"}, {AV_CODEC_ID_H264, "H264"}, {AV_CODEC_ID_HEVC, "H265"} };
    static const std::string defaultCodec = "JPEG";
    const auto it = kMedia2EncodingMap.find(codecId);

    encoder.Encoding = (it != kMedia2EncodingMap.end()) ? it->second : defaultCodec;
#if 1

    ///if (encoder.H264 == 0)
    ///    encoder.H264 = m_tmpH264Conf.get();

    if (!encoder.GovLength)
    {
        if (!m_govLength)
            m_govLength.reset(new int);
        encoder.GovLength = m_govLength.get();
    }
    *encoder.GovLength = qBound(capabilities.govMin, DEFAULT_IFRAME_DISTANCE, capabilities.govMax);

    if (capabilities.encoding == UnderstandableVideoCodec::H264)
    {
        auto desiredH264Profile = resourceData.value<QString>(Qn::DESIRED_H264_PROFILE_PARAM_NAME);

        if (!encoder.Profile)
        {
            if (!m_profile)
                m_profile.reset(new std::string);
            encoder.Profile = m_profile.get();
        }

        if (!desiredH264Profile.isEmpty())
        {
            *encoder.Profile = desiredH264Profile.toStdString();
        }
        else
        {
            if (capabilities.h264Profiles.size())
            {
                *encoder.Profile = H264ProfileToString(capabilities.h264Profiles[0]);
            }
        }
    }
    else if (capabilities.encoding == UnderstandableVideoCodec::H265)
    {
        if (capabilities.h265Profiles.size())
        {
            if (!encoder.Profile)
            {
                if (!m_profile)
                    m_profile.reset(new std::string);
                encoder.Profile = m_profile.get();
            }
            *encoder.Profile = VideoEncoderProfileToString(capabilities.h265Profiles[0]);
        }
    }

    if (!encoder.RateControl)
    {
        // LOG.
    }
    else
    {
        if (!useEncodingInterval)
        {
            encoder.RateControl->FrameRateLimit = params.fps;
        }
        else
        {
            int fpsBase = resourceData.value<int>(Qn::FPS_BASE_PARAM_NAME);
            params.fps = getClosestAvailableFps(params.fps);
            encoder.RateControl->FrameRateLimit = fpsBase;
            //encoder.RateControl->EncodingInterval = static_cast<int>(
            //    fpsBase / params.fps + 0.5);
        }

        encoder.RateControl->BitrateLimit = params.bitrateKbps;
    }

    const Qn::StreamQuality quality = params.quality;
    if (quality != Qn::StreamQuality::preset)
        encoder.Quality = innerQualityToOnvif(quality, capabilities.minQ, capabilities.maxQ);

    if (!encoder.Resolution)
    {
        // LOG.
    }
    else
    {
        encoder.Resolution->Width = params.resolution.width();
        encoder.Resolution->Height = params.resolution.height();
    }
#endif
}

QnPlOnvifResource::VideoOptionsLocal QnPlOnvifResource::primaryVideoCapabilities() const
{
    QnMutexLocker lock(&m_mutex);
    return m_primaryStreamCapabilities;
}

QnPlOnvifResource::VideoOptionsLocal QnPlOnvifResource::secondaryVideoCapabilities() const
{
    QnMutexLocker lock(&m_mutex);
    return m_secondaryStreamCapabilities;
}

SoapTimeouts QnPlOnvifResource::onvifTimeouts() const
{
    return SoapTimeouts(serverModule()->settings().onvifTimeouts());
}

#endif //ENABLE_ONVIF
