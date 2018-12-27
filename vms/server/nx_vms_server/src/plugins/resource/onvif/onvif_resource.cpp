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

// !If renew subscription exactly at termination time, camera can already terminate subscription,
// so have to do that a little bit earlier..
static const unsigned int RENEW_NOTIFICATION_FORWARDING_SECS = 5;
static const unsigned int MS_PER_SECOND = 1000;
static const unsigned int PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC = 1;
static const int MAX_IO_PORTS_PER_DEVICE = 200;
static const int DEFAULT_SOAP_TIMEOUT = 10;
static const quint32 MAX_TIME_DRIFT_UPDATE_PERIOD_MS = 15 * 60 * 1000; // 15 minutes

static const std::string kOnvifMedia2Namespace("http://www.onvif.org/ver20/media/wsdl");

static const QString kBaselineH264Profile("Baseline");
static const QString kMainH264Profile("Main");
static const QString kExtendedH264Profile("Extended");
static const QString kHighH264Profile("High");

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

void updateTimer(nx::utils::TimerId* timerId, std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void(nx::utils::TimerId)> function)
{
    if (*timerId != 0)
    {
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(*timerId);
        *timerId = 0;
    }

    *timerId = nx::utils::TimerManager::instance()->addTimer(
        std::move(function), timeout);
}

/* Some cameras declare invalid max resolution */
struct StrictResolution
{
    const char* model = nullptr;
    QSize maxRes;
};

// strict maximum resolution for this models
// TODO: #Elric #VASILENKO move out to JSON
StrictResolution strictResolutionList[] =
{
    { "Brickcom-30xN", QSize(1920, 1080) }
};

} // namespace

const QString QnPlOnvifResource::MANUFACTURE(lit("OnvifDevice"));
const char* QnPlOnvifResource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifResource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifResource::DEFAULT_IFRAME_DISTANCE = 20;
const float QnPlOnvifResource::QUALITY_COEF = 0.2f;
const int QnPlOnvifResource::MAX_AUDIO_BITRATE = 64; //kbps
const int QnPlOnvifResource::MAX_AUDIO_SAMPLERATE = 32; //khz
const int QnPlOnvifResource::ADVANCED_SETTINGS_VALID_TIME = 60; //60s
static const unsigned int DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT = 30;

//-------------------------------------------------------------------------------------------------
// QnOnvifServiceUrls

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

//-------------------------------------------------------------------------------------------------
// QnPlOnvifResource::VideoEncoderCapabilities

QnPlOnvifResource::VideoEncoderCapabilities::VideoEncoderCapabilities(
    const QString& videoEncoderToken,
    const onvifXsd__VideoEncoderConfigurationOptions& options,
    QnBounds frameRateBounds)
    :
    videoEncoderToken(videoEncoderToken)
{
    if (options.H264)
    {
        encoding = UnderstandableVideoCodec::H264;
        for (const auto& resolution: options.H264->ResolutionsAvailable)
        {
            if (resolution)
                resolutions << QSize(resolution->Width, resolution->Height);
        }

        for (const auto& profile: options.H264->H264ProfilesSupported)
            h264Profiles << profile;
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

        for (const auto& resolution: options.JPEG->ResolutionsAvailable)
        {
            if (resolution)
                resolutions << QSize(resolution->Width, resolution->Height);
        }

        if (options.JPEG->FrameRateRange)
        {
            frameRateMax = restrictFrameRate(
                options.JPEG->FrameRateRange->Max, frameRateBounds);
            frameRateMin = restrictFrameRate(
                options.JPEG->FrameRateRange->Min, frameRateBounds);
        }
    }
    else if (options.MPEG4)
    {
        NX_DEBUG(this, "Device has MPEG4 video encoder, but server ignores it.");
    }

    if (options.QualityRange)
    {
        minQ = options.QualityRange->Min;
        maxQ = options.QualityRange->Max;
    }
}

QnPlOnvifResource::VideoEncoderCapabilities::VideoEncoderCapabilities(
    const QString& videoEncoderToken,
    const onvifXsd__VideoEncoder2ConfigurationOptions& resp,
    QnBounds frameRateBounds)
    :
    videoEncoderToken(videoEncoderToken)
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
    else if (encoding == UnderstandableVideoCodec::H265)
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

std::vector<QnPlOnvifResource::VideoEncoderCapabilities>
    QnPlOnvifResource::VideoEncoderCapabilities::createVideoEncoderCapabilitiesList(
        const QString& videoEncoderToken,
        const onvifXsd__VideoEncoderConfigurationOptions& options,
        QnBounds frameRateBounds)
{
    std::vector<QnPlOnvifResource::VideoEncoderCapabilities> result;
    constexpr int kMaxCount = 2; // H264 + JPEG
    result.reserve(kMaxCount);

    result.push_back(VideoEncoderCapabilities(videoEncoderToken, options, frameRateBounds));

    if (options.H264 && options.JPEG)
    {
        // This is the shallow copy: data on pointers isn't copied.
        onvifXsd__VideoEncoderConfigurationOptions tmpOptions = options;
        tmpOptions.H264 = nullptr;
        result.push_back(VideoEncoderCapabilities(videoEncoderToken, tmpOptions, frameRateBounds));
    }
    return result;
}

int QnPlOnvifResource::VideoEncoderCapabilities::restrictFrameRate(
    int frameRate, QnBounds frameRateBounds) const
{
    if (frameRateBounds.isNull())
        return frameRate;

    return qBound((int) frameRateBounds.min, frameRate, (int) frameRateBounds.max);
}

//-------------------------------------------------------------------------------------------------

namespace {

using VideoEncoderCapabilitiesComparator = std::function<bool(
    const QnPlOnvifResource::VideoEncoderCapabilities&,
    const QnPlOnvifResource::VideoEncoderCapabilities&)>;

bool VideoEncoderCapabilitiesGreaterThan(
    const QnPlOnvifResource::VideoEncoderCapabilities &s1,
    const QnPlOnvifResource::VideoEncoderCapabilities &s2)
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

    // For equal resolutions the rule is: H265 > H264 > JPEG.
    if (s1.encoding != s2.encoding)
        return s1.encoding > s2.encoding;

    if (!s1.isUsedInProfiles && s2.isUsedInProfiles)
        return false;
    else if (s1.isUsedInProfiles && !s2.isUsedInProfiles)
        return true;

    return s1.videoEncoderToken < s2.videoEncoderToken; // sort by name
}

bool compareByProfiles(
    const QnPlOnvifResource::VideoEncoderCapabilities &s1,
    const QnPlOnvifResource::VideoEncoderCapabilities &s2,
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

    return VideoEncoderCapabilitiesGreaterThan(s1, s2);
}

VideoEncoderCapabilitiesComparator createComparator(const QString& profiles)
{
    if (!profiles.isEmpty())
    {
        QStringList profileList = profiles.split(L',');
        QMap<QString, int> profilePriorities;
        for (auto i = 0; i < profileList.size(); ++i)
            profilePriorities[profileList[i]] = profileList.size() - i;

        return
            [profilePriorities](
                const QnPlOnvifResource::VideoEncoderCapabilities &s1,
                const QnPlOnvifResource::VideoEncoderCapabilities &s2) -> bool
            {
                return compareByProfiles(s1, s2, profilePriorities);
            };
    }

    return VideoEncoderCapabilitiesGreaterThan;
}
} // namespace

//-------------------------------------------------------------------------------------------------
// QnPlOnvifResource

QnPlOnvifResource::QnPlOnvifResource(QnMediaServerModule* serverModule):
    base_type(serverModule),
    m_audioCodec(AUDIO_NONE),
    m_audioBitrate(0),
    m_audioSamplerate(0),
    m_timeDrift(0),
    m_isRelayOutputInversed(false),
    m_fixWrongInputPortNumber(false),
    m_fixWrongOutputPortToken(false),
    m_inputMonitored(false),
    m_clearInputsTimeoutUSec(0),
    m_eventMonitorType(emtNone),
    m_nextPullMessagesTimerID(0),
    m_renewSubscriptionTimerID(0),
    m_maxChannels(1),
    m_streamConfCounter(0),
    m_previousPullMessagesResponseTimeMs(0),
    m_inputPortCount(0),
    m_videoLayout(nullptr),
    m_advancedParametersProvider(this),
    m_onvifRecieveTimeout(DEFAULT_SOAP_TIMEOUT),
    m_onvifSendTimeout(DEFAULT_SOAP_TIMEOUT)
{
    OnvifIniConfig::instance().reload();
    m_tmpH264Conf.reset(new onvifXsd__H264Configuration());
    m_pullMessagesResponseElapsedTimer.start();
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

            // Guarantees that no onTimer(timerID) is running on return.
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
                addresses.cbegin();

            for (; addrPtrIter != addresses.end(); ++addrPtrIter)
            {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;
                if (!addrPtr)
                    continue;

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

    nx::vms::server::resource::Camera::setHostAddress(ip);
}

const QString QnPlOnvifResource::createOnvifEndpointUrl(const QString& ipAddress)
{
    return QLatin1String(ONVIF_PROTOCOL_PREFIX) + ipAddress + QLatin1String(ONVIF_URL_SUFFIX);
}

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

    using GSoapDeviceGetNetworkIntfAsyncWrapper = GSoapAsyncCallWrapper<
        DeviceSoapWrapper,
        NetIfacesReq,
        NetIfacesResp>;

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
    auto shouldAppearAsSingleChannel = resData.value<bool>(
        ResourceDataKey::kShouldAppearAsSingleChannel);

    if (shouldAppearAsSingleChannel)
        return new nx::plugins::utils::MultisensorDataProvider(toSharedPointer(this));

    return new QnOnvifStreamReader(toSharedPointer(this));
}

nx::vms::server::resource::StreamCapabilityMap QnPlOnvifResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex streamIndex)
{
    QnMutexLocker lock(&m_mutex);

    const auto& capabilities = (streamIndex == Qn::StreamIndex::primary)
        ? m_primaryStreamCapabilitiesList : m_secondaryStreamCapabilitiesList;

    nx::vms::server::resource::StreamCapabilityMap result;

    static const QMap<UnderstandableVideoCodec, QString> kEncoderNames =
    {
        {UnderstandableVideoCodec::JPEG, QnAvCodecHelper::codecIdToString(AV_CODEC_ID_MJPEG)},
        {UnderstandableVideoCodec::H264, QnAvCodecHelper::codecIdToString(AV_CODEC_ID_H264)},
        {UnderstandableVideoCodec::H265, QnAvCodecHelper::codecIdToString(AV_CODEC_ID_HEVC)},
    };

    for (const auto& extension: capabilities)
    {
        nx::vms::server::resource::StreamCapabilityKey key;
        key.codec = kEncoderNames[extension.encoding];
        for (const auto& resolution: extension.resolutions)
        {
            key.resolution = resolution;
            result.insert(key, nx::media::CameraStreamCapability());
        }
    }

    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::initializeCameraDriver()
{
    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    if (getDeviceOnvifUrl().isEmpty())
    {
        return m_prevOnvifResultCode.errorCode != CameraDiagnostics::ErrorCode::noError
            ? m_prevOnvifResultCode
            : CameraDiagnostics::RequestFailedResult("getDeviceOnvifUrl", QString());
    }

    setCameraCapability(Qn::customMediaPortCapability, true);

    calcTimeDrift();
    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    updateFirmware();
    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    const QAuthenticator auth = getAuth();
    DeviceSoapWrapper deviceSoapWrapper(
        onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);
    _onvifDevice__GetCapabilitiesResponse capabilitiesResponse;
    /*
     Warning! The capabilitiesResponse lifetime must be not more then deviceSoapWrapper lifetime,
     because DeviceSoapWrapper destructor destroys internals of _onvifDevice__GetCapabilitiesResponse.
    */

    auto result = initOnvifCapabilitiesAndUrls(deviceSoapWrapper, &capabilitiesResponse); //< step 1
    if (!checkResultAndSetStatus(result))
        return result;

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

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

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initOnvifCapabilitiesAndUrls(
    DeviceSoapWrapper& deviceSoapWrapper,
    _onvifDevice__GetCapabilitiesResponse* outCapabilitiesResponse)
{
    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    auto result = fetchOnvifCapabilities(deviceSoapWrapper, outCapabilitiesResponse);
    if (!result)
        return result;

    fillFullUrlInfo(*outCapabilitiesResponse);

    if (getMediaUrl().isEmpty())
        return CameraDiagnostics::CameraInvalidParams("ONVIF media URL is not filled by camera");

    QString media2ServiceUrl;
    fetchOnvifMedia2Url(&media2ServiceUrl); //< We ignore the result,
    // because old devices may not support Device::getServices request.

    setMedia2Url(media2ServiceUrl);

    return result;
}

CameraDiagnostics::Result QnPlOnvifResource::initializeMedia(
    const _onvifDevice__GetCapabilitiesResponse& /*onvifCapabilities*/)
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
    const _onvifDevice__GetCapabilitiesResponse& /*onvifCapabilities*/)
{
    const bool result = fetchPtzInfo();
    if (!result)
    {
        return CameraDiagnostics::RequestFailedResult(
            "Fetch ONVIF PTZ configurations.",
            "Can not fetch ONVIF PTZ configurations.");
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::initializeIo(
    const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities)
{
    const QnResourceData resourceData = this->resourceData();
    m_inputPortCount = 0;
    m_isRelayOutputInversed = resourceData.value(QString("relayOutputInversed"), false);
    m_fixWrongInputPortNumber = resourceData.value(QString("fixWrongInputPortNumber"), false);
    m_fixWrongOutputPortToken = resourceData.value(QString("fixWrongOutputPortToken"), false);

    // Registering ONVIF event handler.
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
                NX_WARNING(this, "1%(): Device %2 (%3) reports too many input ports (%4).",
                    __func__, getName(), getId(), portCount);
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
    const _onvifDevice__GetCapabilitiesResponse& /*onvifCapabilities*/)
{
    fetchAndSetAdvancedParameters();
    return CameraDiagnostics::NoErrorResult();
}

int QnPlOnvifResource::suggestBitrateKbps(
    const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    return strictBitrate(
        nx::vms::server::resource::Camera::suggestBitrateKbps(streamParams, role), role);
}

int QnPlOnvifResource::strictBitrate(int bitrate, Qn::ConnectionRole role) const
{
    auto resData = resourceData();

    QString availableBitratesParamName;
    QString bitrateBoundsParamName;

    quint64 bestBitrate = bitrate;

    if (role == Qn::CR_LiveVideo)
    {
        bitrateBoundsParamName = ResourceDataKey::kHighStreamBitrateBounds;
        availableBitratesParamName = ResourceDataKey::kHighStreamAvailableBitrates;
    }
    else if (role == Qn::CR_SecondaryLiveVideo)
    {
        bitrateBoundsParamName = ResourceDataKey::kLowStreamBitrateBounds;
        availableBitratesParamName = ResourceDataKey::kLowStreamAvailableBitrates;
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
        ResourceDataKey::kForcedSecondaryStreamResolution);

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
                   "(ResourceDataKey::kForcedSecondaryStreamResolution) %1"),
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
        m_serviceUrls.deviceServiceUrl = getProperty(ResourcePropertyKey::Onvif::kDeviceUrl);

    return m_serviceUrls.deviceServiceUrl;
}

void QnPlOnvifResource::setDeviceOnvifUrl(const QString& src)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_serviceUrls.deviceServiceUrl = src;
    }

    setProperty(ResourcePropertyKey::Onvif::kDeviceUrl, src);
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
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapFailMessage(
            soapWrapper, "GetDeviceInformation", soapRes));

        if (soapWrapper.lastErrorIsNotAuthenticated())
            return CameraDiagnostics::NotAuthorisedResult(onvifUrl);

        return CameraDiagnostics::RequestFailedResult(
            "getDeviceInformation", soapWrapper.getLastErrorDescription());
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
        // TODO: debug output should be revised in 4.1.
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapFailMessage(
            soapWrapper, "GetNetworkInterfaces", soapRes));
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

const char* QnPlOnvifResource::attributeTextByName(
    const soap_dom_element* element, const char* attributeName)
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

QnPlOnvifResource::onvifSimpleItem  QnPlOnvifResource::parseSimpleItem(
    const soap_dom_element* element)
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

std::vector<QnPlOnvifResource::onvifSimpleItem> QnPlOnvifResource::parseChildSimpleItems(
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

void QnPlOnvifResource::handleOneNotification(
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

    const auto topicsToCheck = notificationTopicsForMonitoring();
    const bool topicIsFound = std::any_of(
        topicsToCheck.cbegin(),
        topicsToCheck.cend(),
        [&eventTopic](const QString& topic) { return eventTopic.indexOf(topic) != -1; });

    if (!topicIsFound)
    {
        NX_VERBOSE(this, "Received notification with unknown topic: %1. Notification ignored.",
            notification.Topic->__any.text);
        return;
    }

    // parsing Message
    soap_dom_attribute* att = notification.Message.__any.atts;
    QString text;
    while (att && att->name && att->text)
    {
        if (QByteArray(att->name).toLower() == "utctime")
            text = QString(att->text); // example: "2018-04-30T21:18:37Z"
        att = att->next;
    }

    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (dateTime.timeSpec() == Qt::LocalTime)
        dateTime.setTimeZone(m_cameraTimeZone);
    if (dateTime.timeSpec() != Qt::UTC)
        dateTime = dateTime.toUTC();

    const time_t notificationTime = dateTime.toTime_t();

    if ((minNotificationTime != (time_t)-1) && (notificationTime < minNotificationTime))
    {
        // DW camera can deliver old cached notifications. We ignore them.
        return;
    }

    std::vector<onvifSimpleItem> source;
    onvifSimpleItem data;
    parseSourceAndData(&notification.Message.__any, &source, &data);

    bool sourceIsExplicitRelayInput = false;

    //checking that there is single source and this source is a relay port
    auto portSourceIter = source.cend();
    const auto inputSourceNames = allowedInputSourceNames();
    for (auto it = source.cbegin(); it != source.cend(); ++it)
    {
        const auto name = QString::fromStdString(it->name);
        if (inputSourceNames.find(name.toLower()) != inputSourceNames.cend())
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

    static const std::set<QString> kStateStrings{
        "logicalstate",
        "state",
        "level",
        "relaylogicalstate"};

    if (portSourceIter == source.end()  //< source is not port
        || kStateStrings.find(QString::fromStdString(data.name).toLower()) == kStateStrings.cend())
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
            [value = QString::fromStdString(source.front().value).toLower(),
                isWrongToken = m_fixWrongOutputPortToken]
            (const RelayOutputInfo& outputInfo)
            {
                QString token = QString::fromStdString(outputInfo.token).toLower();
                if (isWrongToken && !token.isEmpty())
                    token = token.split('-').back();
                return  token == value;
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
        // ONVIF device enumerates ports from 1. see 'allPorts' filling code.
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

    NX_DEBUG(this, "Input port '%1' = %2", portId, state.value);
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

    result = updateResourceCapabilities();
    if (!result)
        return result;

    // Before invoking fetchAndSetHasDualStreaming() primary and secondary Resolutions MUST be set.
    fetchAndSetDualStreaming();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetAudioResourceOptions()
{
    MediaSoapWrapper soapWrapper(this);

    if (fetchAndSetAudioEncoder(soapWrapper) && fetchAndSetAudioEncoderOptions(soapWrapper))
        setProperty(ResourcePropertyKey::kIsAudioSupported, QString("1"));
    else
        setProperty(ResourcePropertyKey::kIsAudioSupported, QString("0"));

    return CameraDiagnostics::NoErrorResult();
}

int QnPlOnvifResource::innerQualityToOnvif(
    Qn::StreamQuality quality, int minQuality, int maxQuality) const
{
    if (quality > Qn::StreamQuality::highest)
    {
        NX_VERBOSE(this,
            "innerQualityToOnvif: got unexpected quality (too big): %1", (int) quality);
        return maxQuality;
    }
    if (quality < Qn::StreamQuality::lowest)
    {
        NX_VERBOSE(this,
            "innerQualityToOnvif: got unexpected quality (too small): %1", (int) quality);
        return minQuality;
    }

    int onvifQuality = minQuality
        + (maxQuality - minQuality)
        * ((int)quality - (int)Qn::StreamQuality::lowest)
        / ((int)Qn::StreamQuality::highest - (int)Qn::StreamQuality::lowest);

    NX_DEBUG(this, "innerQualityToOnvif: in quality = %1, out qualty = %2, minOnvifQuality = %3, maxOnvifQuality = %4",
        (int) quality, onvifQuality, minQuality, maxQuality);

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

    static const QString kRequestCommand("GetSystemDateAndTime");
    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapFailMessage(
            soapWrapper, kRequestCommand, soapRes));

        return 0;
    }
    if (!response.SystemDateAndTime)
    {
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapNoParameterMessage(
            soapWrapper, "SystemDateAndTime", __func__, kRequestCommand));
        return 0;
    }
    if (!response.SystemDateAndTime->UTCDateTime)
    {
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapNoParameterMessage(
            soapWrapper, "SystemDateAndTime->UTCDateTime", __func__, kRequestCommand));
        return 0;
    }

    if (timeZone && response.SystemDateAndTime->TimeZone)
        *timeZone = QTimeZone(response.SystemDateAndTime->TimeZone->TZ.c_str());

    onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
    if (!date)
    {
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapNoParameterMessage(
            soapWrapper, "SystemDateAndTime->UTCDateTime->Date", __func__, kRequestCommand));
        return 0;
    }
    onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;
    if (!time)
    {
        NX_DEBUG(NX_SCOPE_TAG, makeStaticSoapNoParameterMessage(
            soapWrapper, "SystemDateAndTime->UTCDateTime->Time", __func__, kRequestCommand));
        return 0;
    }

    QDateTime datetime(
        QDate(date->Year, date->Month, date->Day),
        QTime(time->Hour, time->Minute, time->Second),
        Qt::UTC);
    int drift = datetime.toMSecsSinceEpoch()/MS_PER_SECOND
        - QDateTime::currentMSecsSinceEpoch()/MS_PER_SECOND;
    return drift;
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
    if (resourceData().contains("forcedOnvifParams"))
    {
        const auto channel = getChannel();
        const auto forcedOnvifParams =
            resourceData().value<QnOnvifConfigDataPtr>(QString("forcedOnvifParams"));

        if (forcedOnvifParams->ptzProfiles.size() > channel)
            return forcedOnvifParams->ptzProfiles.at(channel);
    }

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

    bool result = nx::vms::server::resource::Camera::mergeResourcesIfNeeded(source);

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

bool QnPlOnvifResource::fetchRelayInputInfo(const _onvifDevice__GetCapabilitiesResponse& capabilitiesResponse)
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
    m_portAliases = resData.value<std::vector<QString>>(ResourceDataKey::kOnvifInputPortAliases);

    if (!m_portAliases.empty())
        return true;

    DeviceIO::DigitalInputs digitalInputs(this);
    digitalInputs.receiveBySoap();

    if (!digitalInputs && digitalInputs.soapError() != SOAP_MUSTUNDERSTAND)
    {
        NX_DEBUG(this, makeSoapFailMessage(digitalInputs.innerWrapper(),
            __func__, "GetDigitalInputs", digitalInputs.soapError()));
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
        NX_DEBUG(this, makeFailMessage("PTZ web service is not supported"));
        return false;
    }

    if (resourceData().contains("forcedOnvifParams"))
    {
        const auto channel = getChannel();
        const auto forcedOnvifParams =
            resourceData().value<QnOnvifConfigDataPtr>(QString("forcedOnvifParams"));

        if (forcedOnvifParams->ptzConfigurations.size() > channel)
        {
            m_ptzConfigurationToken = forcedOnvifParams->ptzConfigurations.at(channel);
            return true;
        }
    }
    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    int soapRes = ptz.doGetConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            ptz, __func__, "GetConfigurations", soapRes));
        return false;
    }
    if (response.PTZConfiguration.empty())
    {
        NX_DEBUG(this, makeSoapNoParameterMessage(
            ptz, "PTZConfiguration", __func__, "GetConfigurations"));

        return false;
    }

    m_ptzConfigurationToken = QString::fromStdString(response.PTZConfiguration[0]->token);
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
    const VideoEncoderCapabilities& videoEncoderCapabilities)
{
    auto resData = resourceData();

    auto desiredH264Profile = resData.value<QString>(ResourceDataKey::kDesiredH264Profile);

    if (videoEncoderCapabilities.h264Profiles.isEmpty())
        return boost::optional<onvifXsd__H264Profile>();
    else if (!desiredH264Profile.isEmpty())
        return fromStringToH264Profile(desiredH264Profile);
    else
        return videoEncoderCapabilities.h264Profiles[0];
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
int QnPlOnvifResource::getSecondaryIndex(const QList<VideoEncoderCapabilities>& optList) const
{
    NX_ASSERT(optList.size() >= 2);
    if (optList[0].resolutions.isEmpty())
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
    if (!m_inputMonitored)
        return;

    if (timeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS)
        timeoutSec -= RENEW_NOTIFICATION_FORWARDING_SECS;

    const std::chrono::seconds timeout(timeoutSec);
    NX_VERBOSE(this, lm("Schedule renew subscription in %1").arg(timeout));
    updateTimer(&m_renewSubscriptionTimerID, timeout,
        [this](quint64 timerID) { this->onRenewSubscriptionTimer(timerID); });
        //std::bind(&QnPlOnvifResource::onRenewSubscriptionTimer, this, std::placeholders::_1));
}

CameraDiagnostics::Result QnPlOnvifResource::updateVideoEncoderUsage(
    QList<VideoEncoderCapabilities>& optionsList)
{
    Media::Profiles profiles(this);
    profiles.receiveBySoap();
    if (!profiles)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            profiles.innerWrapper(), __func__, "GetProfiles", profiles.soapError()));

        return profiles.requestFailedResult();
    }

    for (const onvifXsd__Profile* profile: profiles.get()->Profiles)
    {
        if (profile->token.empty() || !profile->VideoEncoderConfiguration)
            continue;
        const QString vEncoderToken = QString::fromStdString(profile->VideoEncoderConfiguration->token);
        for (int i = 0; i < optionsList.size(); ++i)
        {
            if (optionsList[i].videoEncoderToken == vEncoderToken)
            {
                optionsList[i].isUsedInProfiles = true;
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

bool QnPlOnvifResource::getVideoEncoderTokens(BaseSoapWrapper& soapWrapper,
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

        if (confRangeEnd > (int) configurations.size())
        {
            const QString errorMessage =
                makeFailMessage("Current channel number is %1, that is more then number "
                "of configurations").arg(QString::number(getChannel()));

            NX_DEBUG(this, makeSoapSmallRangeMessage(
                soapWrapper, "configurations", (int) configurations.size(), confRangeEnd, __func__,
                "GetVideoEncoderConfiguration"));
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
    const std::string& token, QList<VideoEncoderCapabilities>* dstOptionsList, const QnBounds& frameRateBounds)
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
            NX_DEBUG(this, makeSoapFailMessage(
                videoEncoderConfigurationOptions.innerWrapper(), __func__,
                "GetVideoEncoderConfigurationOptions",
                videoEncoderConfigurationOptions.soapError()));

            return videoEncoderConfigurationOptions.requestFailedResult();
        }
        const onvifXsd__VideoEncoderConfigurationOptions* options =
            videoEncoderConfigurationOptions.get()->Options;
        if (!options)
        {
            NX_DEBUG(this, makeSoapNoParameterMessage(
                videoEncoderConfigurationOptions.innerWrapper(), "options", __func__,
                "GetVideoEncoderConfigurationOptions"));
            return CameraDiagnostics::NoErrorResult();
        }

        if (!options->H264 && !options->JPEG)
        {
            NX_DEBUG(this, makeSoapNoParameterMessage(
                videoEncoderConfigurationOptions.innerWrapper(),
                "options->H264 || options->JPEG", __func__,
                "GetVideoEncoderConfigurationOptions"));
            return CameraDiagnostics::NoErrorResult();
        }

        const auto optionsList = VideoEncoderCapabilities::createVideoEncoderCapabilitiesList(
            QString::fromStdString(token), *options, frameRateBounds);

        for (const auto& options: optionsList)
            *dstOptionsList << options;
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
            NX_DEBUG(this, makeSoapFailMessage(
                videoEncoderConfigurationOptions.innerWrapper(), __func__,
                "GetVideoEncoderConfigurationOptions",
                videoEncoderConfigurationOptions.soapError()));

            return videoEncoderConfigurationOptions.requestFailedResult();
        }

        std::vector<onvifXsd__VideoEncoder2ConfigurationOptions *>& optionsList =
            videoEncoderConfigurationOptions.get()->Options;

        for (const onvifXsd__VideoEncoder2ConfigurationOptions* options: optionsList)
        {
            if (options && VideoCodecFromString(options->Encoding) != UnderstandableVideoCodec::NONE)
            {
                *dstOptionsList << VideoEncoderCapabilities(QString::fromStdString(token), *options, frameRateBounds);
            }
        }

    }

    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::fillStreamCapabilityLists(
    const QList<VideoEncoderCapabilities>& capabilitiesList)
{
    class HasToken
    {
        const QString m_token;
    public:
        HasToken(const QString& token): m_token(token) {}
        bool operator()(const VideoEncoderCapabilities& capabilities) const
        {
            return capabilities.videoEncoderToken == m_token;
        }
    };

    QnMutexLocker lock(&m_mutex);

    m_primaryStreamCapabilitiesList.clear();
    m_secondaryStreamCapabilitiesList.clear();

    if (capabilitiesList.isEmpty())
        return;
    HasToken hasPrimaryToken(capabilitiesList.front().videoEncoderToken);

    std::copy_if(capabilitiesList.cbegin(), capabilitiesList.cend(),
        std::back_inserter(m_primaryStreamCapabilitiesList), hasPrimaryToken);

    const auto it = std::find_if_not(capabilitiesList.cbegin(), capabilitiesList.cend(),
        hasPrimaryToken);
    if (it == capabilitiesList.cend())
        return; //< no secondary token

    HasToken hasSecondaryToken(it->videoEncoderToken);
    std::copy_if(capabilitiesList.cbegin(), capabilitiesList.cend(),
        std::back_inserter(m_secondaryStreamCapabilitiesList), hasSecondaryToken);
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
            NX_DEBUG(this, makeSoapFailMessage(
                videoEncoderConfigurations.innerWrapper(), __func__,
                "GetVideoEncoderConfigurations", videoEncoderConfigurations.soapError()));
            return videoEncoderConfigurations.requestFailedResult();
        }

        auto result = getVideoEncoderTokens(videoEncoderConfigurations.innerWrapper(),
            videoEncoderConfigurations.get()->Configurations, &videoEncodersTokenList);
        if (!result)
            return videoEncoderConfigurations.requestFailedResult();
    }

    // Step 2. Extract video encoder options for every token into optionsList.
    const auto frameRateBounds = resourceData().value<QnBounds>(ResourceDataKey::kFpsBounds, QnBounds());
    QList<VideoEncoderCapabilities> optionsList;
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
        NX_DEBUG(this, makeFailMessage("All video options are empty."));

        return CameraDiagnostics::RequestFailedResult(
            "fetchAndSetVideoEncoderOptions", "no video options");
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
            if (configuration && QString::fromStdString(configuration->token) == optionsList[0].videoEncoderToken)
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

    fillStreamCapabilityLists(optionsList);

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::fetchAndSetDualStreaming()
{
    QnMutexLocker lock(&m_mutex);

    auto resData = resourceData();

    auto forceSingleStream = resData.value<bool>(ResourceDataKey::kForceSingleStream, false);

    const bool dualStreaming =
        !forceSingleStream
        && !m_secondaryStreamCapabilitiesList.empty()
        && !m_secondaryStreamCapabilitiesList.front().resolutions.isEmpty()
        && !m_secondaryStreamCapabilitiesList.front().videoEncoderToken.isEmpty();

    if (dualStreaming)
    {
        NX_DEBUG(this, lit("ONVIF debug: enable dualstreaming for camera %1")
                .arg(getHostAddress()));
    }
    else
    {
        QString reason;
        if (forceSingleStream)
            reason = "single stream mode is forced by driver";
        else if (m_secondaryStreamCapabilitiesList.empty())
            reason = "no secondary encoder";
        else if (m_secondaryStreamCapabilitiesList.front().resolutions.isEmpty())
            reason = "no secondary resolution";

        NX_DEBUG(this, lit("ONVIF debug: disable dualstreaming for camera %1 reason: %2")
                .arg(getHostAddress())
                .arg(reason));
    }

    setProperty(ResourcePropertyKey::kHasDualStreaming, dualStreaming ? 1 : 0);
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
    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "getAudioEncoderConfigurationOptions", soapRes));
        return false;
    }
    if (!response.Options)
    {
        NX_DEBUG(this, makeSoapNoParameterMessage(
            soapWrapper, "options", __func__, "GetAudioEncoderConfigurationOptions"));
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
                    const QString errorMessage =
                        makeFailMessage("Unknown codec type. Codec type enum number = %1")
                        .arg(QString::number((int) curOpts->Encoding));

                    NX_DEBUG(this, errorMessage);
                    break;
            }
        }

        ++it;
    }

    if (!options)
    {
        NX_DEBUG(this, makeFailMessage("Codec type is not set"));
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
        bitRate = findClosestRateFloor(options.BitrateList->Items, MAX_AUDIO_BITRATE);
    else
        NX_DEBUG(this, makeFailMessage("Camera didn't return BitrateList"));

    int sampleRate = 0;
    if (options.SampleRateList)
        sampleRate = findClosestRateFloor(options.SampleRateList->Items, MAX_AUDIO_SAMPLERATE);
    else
        NX_DEBUG(this, makeFailMessage("Camera didn't return SampleRateList List"));

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

    if (!m_videoSourceSize.isValid())
        return CameraDiagnostics::NoErrorResult();

    NX_DEBUG(this, QString(lit("ONVIF debug: videoSourceSize is %1x%2 for camera %3")).
        arg(m_videoSourceSize.width()).arg(m_videoSourceSize.height())
        .arg(getHostAddress()));

    class IsResolutionTooBig
    {
        const QSize m_maxResolution;
    public:
        IsResolutionTooBig(QSize maxResolution): m_maxResolution(maxResolution) {}
        bool operator()(QSize resolution) const
        {
            return resolution.width() > m_maxResolution.width()
             || resolution.height() > m_maxResolution.height();
        }
    };

    bool trustToVideoSourceSize = false;
    for (const VideoEncoderCapabilities& capabilities: m_primaryStreamCapabilitiesList)
    {
        const auto it = std::find_if_not(
            capabilities.resolutions.cbegin(), capabilities.resolutions.cend(),
            IsResolutionTooBig(m_videoSourceSize));
        if (it != capabilities.resolutions.cend())
        {
            // Trust to videoSourceSize, if
            // at least 1 appropriate resolution exists for at least one encoder configuration.
            trustToVideoSourceSize = true;
            break;
        }
    }

    bool videoSourceSizeIsRight = resourceData().value<bool>(
        ResourceDataKey::kTrustToVideoSourceSize, true);

    if (!videoSourceSizeIsRight)
        trustToVideoSourceSize = false;

    if (!trustToVideoSourceSize)
    {
        NX_DEBUG(this, QString(lit("ONVIF debug: do not trust to videoSourceSize is %1x%2 for camera %3 because it blocks all resolutions")).
            arg(m_videoSourceSize.width()).arg(m_videoSourceSize.height()).arg(getHostAddress()));
        return CameraDiagnostics::NoErrorResult();
    }

    // Filter out all resolutions from all video encoder configuration options, that are larger
    // then m_videoSourceSize
    for (VideoEncoderCapabilities& capabilities: m_primaryStreamCapabilitiesList)
    {
        const auto it = std::remove_if(
            capabilities.resolutions.begin(), capabilities.resolutions.end(),
            IsResolutionTooBig(m_videoSourceSize));
        capabilities.resolutions.erase(it, capabilities.resolutions.end());
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper)
{
    AudioConfigsReq request;
    AudioConfigsResp response;
    static const QString kRequestCommand("GetAudioEncoderConfigurations");

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, kRequestCommand, soapRes));
        return false;
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(
            soapWrapper, __func__, kRequestCommand));
    }

    if (response.Configurations.empty())
    {
        NX_DEBUG(this, makeSoapNoParameterMessage(
            soapWrapper, "configurations", __func__, kRequestCommand));

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
                m_audioEncoderId = QString::fromStdString(conf->token);
            }
        }
        else
        {
            NX_DEBUG(this, makeSoapNoRangeParameterMessage(
                soapWrapper, "configurations", getChannel(), __func__, kRequestCommand));
            return false;
        }
    }

    return true;
}

void QnPlOnvifResource::updateVideoSource(VideoSource* source, const QRect& maxRect) const
{
    //One name for primary and secondary
    //source.Name = NETOPTIX_PRIMARY_NAME;
    NX_ASSERT(source->Bounds);

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
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "SetVideoSourceConfiguration", soapRes));

        if (soapWrapper.lastErrorIsNotAuthenticated())
            setStatus(Qn::Unauthorized);

        // Ignore error because of some cameras is not ONVIF profile S compatible
        // and doesn't support this request.
        return CameraDiagnostics::NoErrorResult();
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlOnvifResource::fetchChannelCount(bool limitedByEncoders)
{
    MediaSoapWrapper soapWrapper(this);
    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);
    static const QString kRequestCommand("GetVideoSources");

    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, kRequestCommand, soapRes));

        if (soapWrapper.lastErrorIsNotAuthenticated())
            return CameraDiagnostics::NotAuthorisedResult(getMediaUrl());

        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getVideoSources"), soapWrapper.getLastErrorDescription());
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(soapWrapper, __func__, kRequestCommand));
    }

    m_maxChannels = (int) response.VideoSources.size();
    int thisChannelNumber = getChannel();

    if (m_maxChannels <= thisChannelNumber)
    {
        const QString errorMessage = makeSoapSmallRangeMessage(
            soapWrapper, "VideoSources", m_maxChannels, thisChannelNumber + 1,
            __func__, kRequestCommand);
        NX_DEBUG(this, errorMessage);
        return CameraDiagnostics::RequestFailedResult("getVideoSources", errorMessage);
    }

    onvifXsd__VideoSource* conf = response.VideoSources[thisChannelNumber];

    if (!conf)
    {
        const QString errorMessage = makeSoapNoRangeParameterMessage(
            soapWrapper, "VideoSources", thisChannelNumber, __func__, kRequestCommand);
        NX_DEBUG(this, errorMessage);
        return CameraDiagnostics::RequestFailedResult("getVideoSources", errorMessage);
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
            NX_DEBUG(this, makeSoapFailMessage(
                soapWrapper, __func__, "GetVideoEncoderConfigurations", soapRes));

            return CameraDiagnostics::RequestFailedResult(
                "getVideoEncoderConfigurations", soapWrapper.getLastErrorDescription());
        }
        else
        {
            NX_VERBOSE(this, makeSoapSuccessMessage(
                soapWrapper, __func__, "getVideoEncoderConfigurations"));
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
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetVideoSourceConfigurationOptions", soapRes));
        return QRect();
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(
            soapWrapper, __func__, "GetVideoSourceConfigurationOptions"));
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
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetVideoSourceConfigurations", soapRes));

        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getVideoSourceConfigurations"), soapWrapper.getLastErrorDescription());
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(soapWrapper, __func__, "GetVideoSourceConfigurations"));
    }

    if (commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    std::string srcToken = m_videoSourceToken.toStdString();
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        onvifXsd__VideoSourceConfiguration* conf = response.Configurations[i];
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
    static const QString kRequestCommand("GetAudioSourceConfigurations");

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, kRequestCommand, soapRes));

        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getAudioSourceConfigurations"), soapWrapper.getLastErrorDescription());
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(soapWrapper, __func__, kRequestCommand));
    }

    const int thisChannelNumber = getChannel();
    const int channelCount = (int)response.Configurations.size();

    if (channelCount <= thisChannelNumber)
    {
        const QString errorMessage = makeSoapSmallRangeMessage(
            soapWrapper, "Configurations", channelCount, thisChannelNumber,
            __func__, kRequestCommand);
        NX_DEBUG(this, errorMessage);

        return CameraDiagnostics::RequestFailedResult(
            "getAudioSourceConfigurations", errorMessage);
    }

    onvifXsd__AudioSourceConfiguration* conf = response.Configurations[thisChannelNumber];
    if (!conf)
    {
        const QString errorMessage = makeSoapNoRangeParameterMessage(
            soapWrapper, "Configurations", thisChannelNumber,
            __func__, kRequestCommand);
        NX_DEBUG(this, errorMessage);
        return CameraDiagnostics::RequestFailedResult(
            "getAudioSourceConfigurations", errorMessage);
    }

    QnMutexLocker lock(&m_mutex);
    m_audioSourceId = QString::fromStdString(conf->token);
    return CameraDiagnostics::NoErrorResult();
}

std::set<QString> QnPlOnvifResource::notificationTopicsForMonitoring() const
{
    std::set<QString> result{
        "Trigger/Relay", "IO/Port", "Trigger/DigitalInput", "Device/IO/VirtualPort"
    };
    const auto additionalNotificationTopics = resourceData().value<std::vector<QString>>(
        "additionalNotificationTopics");

    result.insert(additionalNotificationTopics.cbegin(), additionalNotificationTopics.cend());
    return result;
}

std::set<QString> QnPlOnvifResource::allowedInputSourceNames() const
{
    std::set<QString> result{"port", "relaytoken", "index"};
    const auto additionalInputSourceNames = resourceData().value<std::vector<QString>>(
        "additionalInputSourceNames");

    result.insert(additionalInputSourceNames.cbegin(), additionalInputSourceNames.cend());
    return result;
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

std::vector<nx::vms::server::resource::Camera::AdvancedParametersProvider*>
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
        NX_DEBUG(this, makeSoapFailMessage(
            vecSetter.innerWrapper(), __func__, "SetVideoEncoderConfiguration", vecSetter.soapError()));

        if (vecSetter.innerWrapper().lastErrorIsNotAuthenticated())
            setStatus(Qn::Unauthorized);
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
        int t = 0;
        NX_DEBUG(this, makeSoapFailMessage(
            vecSetter.innerWrapper(), __func__, "SetVideoEncoderConfiguration", vecSetter.soapError()));

        if (vecSetter.innerWrapper().lastErrorIsNotAuthenticated())
            setStatus(Qn::Unauthorized);
        if (vecSetter.innerWrapper().getLastErrorDescription().contains("not possible to set"))
            return CameraDiagnostics::CannotConfigureMediaStreamResult("fps");   // TODO: #ak find param name
        else
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QString("'stream profile parameters'")); //CameraDiagnostics::NoErrorResult(); //
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
    if (!configuration || !configuration->RateControl || !configuration->Resolution)
        return;

    if (m_primaryStreamCapabilitiesList.empty())
        return;

    const VideoEncoderCapabilities& primaryStreamCapabilities =
        m_primaryStreamCapabilitiesList.front();

    if (primaryStreamCapabilities.resolutions.isEmpty())
        return;

    // This code seems to be never executed. (???)
    int maxFpsOrig = getMaxFps();
    int rangeHi = getMaxFps()-2;
    int rangeLow = getMaxFps()/4;
    int currentFps = rangeHi;
    int prevFpsValue = -1;

    QSize resolution = primaryStreamCapabilities.resolutions[0];
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

    return nullptr;
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
        if (!m_inputMonitored)
            return;

        m_inputMonitored = false;
        nextPullMessagesTimerIDBak = m_nextPullMessagesTimerID;
        m_nextPullMessagesTimerID = 0;
        renewSubscriptionTimerIDBak = m_renewSubscriptionTimerID;
        m_renewSubscriptionTimerID = 0;
    }

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
        QnMutexLocker lk(&m_ioPortMutex);
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
        const bool updatePort =
            OnvifIniConfig::instance().doUpdatePortInSubscriptionAddress;
        m_onvifNotificationSubscriptionReference =
            fromOnvifDiscoveredUrl(response.SubscriptionReference.Address, updatePort);
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
    m_previousPullMessagesResponseTimeMs = m_pullMessagesResponseElapsedTimer.elapsed();

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

    _onvifEvents__PullMessages request;
    request.Timeout = (m_pullMessagesResponseElapsedTimer.elapsed() -
        m_previousPullMessagesResponseTimeMs) / MS_PER_SECOND * MS_PER_SECOND;
    request.MessageLimit = MAX_MESSAGES_TO_PULL;

    std::vector<void*> memToFreeOnResponseDone;
    constexpr int kExperctedAllocationsCount = 4;
    memToFreeOnResponseDone.reserve(kExperctedAllocationsCount);

    struct SOAP_ENV__Header* header = (struct SOAP_ENV__Header*) malloc(sizeof(SOAP_ENV__Header));
    memToFreeOnResponseDone.push_back(header);
    memset(header, 0, sizeof(*header));
    soapWrapper->soap()->header = header;

    if(!m_onvifNotificationSubscriptionID.isEmpty())
    {
        QByteArray onvifNotificationSubscriptionIDLatin1 = m_onvifNotificationSubscriptionID.toLatin1();
        char* SubscriptionIdBuf = (char*) malloc(512);
        memToFreeOnResponseDone.push_back(SubscriptionIdBuf);
        strcpy(SubscriptionIdBuf, onvifNotificationSubscriptionIDLatin1.data());
        soapWrapper->soap()->header->SubscriptionId = SubscriptionIdBuf;
    }

    //TODO #ak move away check for "Samsung"
    if (!m_onvifNotificationSubscriptionReference.isEmpty() && !getVendor().contains(lit("Samsung")))
    {
        const QByteArray& onvifNotificationSubscriptionReferenceUtf8 = m_onvifNotificationSubscriptionReference.toUtf8();
        char* toBuf = (char*) malloc(onvifNotificationSubscriptionReferenceUtf8.size()+1);
        memToFreeOnResponseDone.push_back(toBuf);
        strcpy(toBuf, onvifNotificationSubscriptionReferenceUtf8.constData());
        soapWrapper->soap()->header->wsa5__To = toBuf;
    }

    // Very few devices need wsa__Action and wsa__ReplyTo to be filled, but sometimes they do.
    wsa5__EndpointReferenceType* replyTo =
        (wsa5__EndpointReferenceType*) malloc(sizeof(wsa5__EndpointReferenceType));
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
    const auto useHttpReader = resData.value<bool>(
        ResourceDataKey::kParseOnvifNotificationsWithHttpReader,
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

    handleAllNotifications(asyncWrapper->response());

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

void QnPlOnvifResource::handleAllNotifications(const _onvifEvents__PullMessagesResponse& response)
{
    const qint64 currentPullMessagesResponseTimeMs = m_pullMessagesResponseElapsedTimer.elapsed();
    const time_t timeSinceLastResponseSec = roundUp<qint64>(currentPullMessagesResponseTimeMs -
        m_previousPullMessagesResponseTimeMs, MS_PER_SECOND) / MS_PER_SECOND;

    // Notifications with timestamps older then minNotificationTime are ignored.
    const time_t minNotificationTime = response.CurrentTime - timeSinceLastResponseSec;
    for (const auto& notification: response.oasisWsnB2__NotificationMessage)
    {
        if (notification)
            handleOneNotification(*notification, minNotificationTime);
    }
    m_previousPullMessagesResponseTimeMs = currentPullMessagesResponseTimeMs;
}

bool QnPlOnvifResource::fetchRelayOutputs(std::vector<RelayOutputInfo>* relayOutputInfoList)
{
#if 0
    /*
        See the comment to DeviceIO::RelayOutputs in header file.
        This code should be tested with different cameras, especially DW.
    */
    DeviceIO::RelayOutputs relayOutputs(this);
    relayOutputs.receiveBySoap();
#endif

    const QAuthenticator auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift);

    _onvifDevice__GetRelayOutputs request;
    _onvifDevice__GetRelayOutputsResponse response;

    int soapRes = soapWrapper.getRelayOutputs(request, response);
    if ((soapRes != SOAP_OK) && (soapRes != SOAP_MUSTUNDERSTAND))
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetRelayOutputs", soapRes));
        return false;
    }

    m_relayOutputInfo.clear();
    if (response.RelayOutputs.size() > MAX_IO_PORTS_PER_DEVICE)
    {
        NX_DEBUG(this, makeFailMessage("Device has too many relay outputs."));
        return false;
    }

    for (const auto& output: response.RelayOutputs)
    {
        if (output)
        {
            m_relayOutputInfo.emplace_back(
                output->token,
                output->Properties->Mode == onvifXsd__RelayMode::Bistable,
                output->Properties->DelayTime,
                output->Properties->IdleState == onvifXsd__RelayIdleState::closed);
        }
    }

    if (relayOutputInfoList)
        *relayOutputInfoList = m_relayOutputInfo;

    NX_DEBUG(this, lit("Successfully got device (%1) output ports info. Found %2 relay output").
        arg(soapWrapper.endpoint()).arg(m_relayOutputInfo.size()));

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
    auto layoutStr = resData.value<QString>(ResourceDataKey::kVideoLayout);
    auto videoLayout = layoutStr.isEmpty()
        ? QnMediaResource::getVideoLayout(dataProvider)
        : QnConstResourceVideoLayoutPtr(QnCustomResourceVideoLayout::fromString(layoutStr));

    auto nonConstThis = const_cast<QnPlOnvifResource*>(this);
    {
        QnMutexLocker lock(&m_layoutMutex);
        m_videoLayout = videoLayout;
        nonConstThis->setProperty(ResourcePropertyKey::kVideoLayout, videoLayout->toString());
        nonConstThis->saveProperties();
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
    auto useEncodingInterval = resData.value<bool>(
        ResourceDataKey::kControlFpsViaEncodingInterval);

    if (useEncodingInterval)
    {
        int fpsBase = resData.value<int>(ResourceDataKey::kfpsBase);
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

    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetDeviceInformation", soapRes));
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(
            soapWrapper, __func__, "GetDeviceInformation"));

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
    _onvifDevice__GetCapabilitiesResponse response;
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

QString QnPlOnvifResource::makeSoapFailMessage(BaseSoapWrapper& soapWrapper,
    const QString& caller, const QString& requestCommand,
    int soapError, const QString& text /*= QString()*/) const
{
    static const QString kSoapErrorMessagePattern =
        "SOAP request failed. %1 Caller = %2(), Device = %3, id = %4, url = %5"
        ", request command = %6, error = %7 (\"%8\")";

    return kSoapErrorMessagePattern.arg(text, caller, getName(), getId().toString(),
        soapWrapper.endpoint(), requestCommand, QString::number(soapError),
        soapWrapper.getLastErrorDescription());
}

QString QnPlOnvifResource::makeSoapSuccessMessage(BaseSoapWrapper& soapWrapper,
    const QString& caller, const QString& requestCommand,
    const QString& text /*= QString()*/) const
{
    static const QString kSoapErrorMessagePattern =
        "SOAP request succeeded. %1 Caller = %2(), Device = %3, id = %4, url = %5"
        ", request command = %6";

    return kSoapErrorMessagePattern.arg(text, caller, getName(), getId().toString(),
        soapWrapper.endpoint(), requestCommand);
}

QString QnPlOnvifResource::makeSoapNoParameterMessage(BaseSoapWrapper& soapWrapper,
    const QString& missedParameter, const QString& caller, const QString& requestCommand,
    const QString& text /*= QString()*/) const
{
    static const QString kSoapErrorMessagePattern =
        "SOAP response has no parameter: Missed parameter = %1. %2 Caller = %3()"
        ", Device = %4, id = %5, url = %6, request command = %7";

    return kSoapErrorMessagePattern.arg(missedParameter, text, caller, getName(),
        getId().toString(), soapWrapper.endpoint(), requestCommand);
}

QString QnPlOnvifResource::makeSoapNoRangeParameterMessage(BaseSoapWrapper& soapWrapper,
    const QString& rangeParameter, int index, const QString& caller,
    const QString& requestCommand, const QString& text /*= QString()*/) const
{
    static const QString kSoapErrorMessagePattern =
        "SOAP response has no parameter: Missed parameter = %1[%2]. %3 Caller = %4()"
        ", Device = %5, id = %6, url = %7, request command = %8";

    return kSoapErrorMessagePattern.arg(rangeParameter, QString::number(index), text, caller,
        getName(), getId().toString(), soapWrapper.endpoint(), requestCommand);
}

QString QnPlOnvifResource::makeSoapSmallRangeMessage(BaseSoapWrapper& soapWrapper,
    const QString& rangeParameter, int rangeSize, int desiredSize,
    const QString& caller, const QString& requestCommand,
    const QString& text /*= QString()*/) const
{
    static const QString kSoapErrorMessagePattern =
        "SOAP response has small range: Range parameter = %1, Range size = %2, Desired size = %3"
        ", %4 Caller = %5(), Device = %6, id = %7, url = %8, request command = %9";

    return kSoapErrorMessagePattern.arg(rangeParameter, QString::number(rangeSize),
        QString::number(desiredSize), text, caller, getName(), getId().toString(),
        soapWrapper.endpoint(), requestCommand);
}

QString QnPlOnvifResource::makeStaticSoapFailMessage(BaseSoapWrapper& soapWrapper,
    const QString& requestCommand, int soapError, const QString& text /*= QString()*/)
{
    static const QString kSoapErrorMessagePattern =
        "SOAP request failed. %1 url = %3, request command = %4"
        ", error = %5 (\"%6\")";

    return kSoapErrorMessagePattern.arg(text, soapWrapper.endpoint(), requestCommand,
        QString::number(soapError), soapWrapper.getLastErrorDescription());
}

QString QnPlOnvifResource::makeStaticSoapNoParameterMessage(BaseSoapWrapper& soapWrapper,
    const QString& missedParameter, const QString& caller, const QString& requestCommand,
    const QString& text /*= QString()*/)
{
    static const QString kSoapErrorMessagePattern =
        "SOAP response is incomplete: Missed parameter = %1. %2 Caller = %3(), url = %6"
        ", request command = %7";

    return kSoapErrorMessagePattern.arg(missedParameter, text, caller,
        soapWrapper.endpoint(), requestCommand);
}

QString QnPlOnvifResource::makeFailMessage(const QString& text) const
{
    return QString("Device %1 (%2). %3").arg(getName(), getId().toString(), text);
}

CameraDiagnostics::Result QnPlOnvifResource::fetchOnvifCapabilities(
    DeviceSoapWrapper& soapWrapper,
    _onvifDevice__GetCapabilitiesResponse* response)
{
    _onvifDevice__GetCapabilities request;
    int soapRes = soapWrapper.getCapabilities(request, *response);

    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetCapabilities", soapRes));

        if (soapWrapper.lastErrorIsNotAuthenticated())
        {
            if (!getId().isNull())
                setStatus(Qn::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult(getDeviceOnvifUrl());
        }
        return CameraDiagnostics::RequestFailedResult(
            "getCapabilities", soapWrapper.getLastErrorDescription());
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(soapWrapper, __func__, "GetCapabilities"));
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

        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetServices", soapRes));
        return CameraDiagnostics::RequestFailedResult("GetServices", soapWrapper.getLastErrorDescription());
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(soapWrapper, __func__, "GetServices"));
    }

    for (const onvifDevice__Service* service: response.Service)
    {
        if (service && service->Namespace == kOnvifMedia2Namespace)
        {
            *url = fromOnvifDiscoveredUrl(service->XAddr);
            return CameraDiagnostics::NoErrorResult();;
        }
    }

    return CameraDiagnostics::RequestFailedResult(
        "GetServices", "Web service \"Media2\" is not found.");
}

void QnPlOnvifResource::fillFullUrlInfo(const _onvifDevice__GetCapabilitiesResponse& response)
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
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return true;

    QString shortModel = model;
    shortModel.replace(QString(lit(" ")), QString());
    shortModel.replace(QString(lit("-")), QString());
    resourceData = dataPool->data(manufacturer, shortModel);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return true;

    if (shortModel.startsWith(manufacturer))
        shortModel = shortModel.mid(manufacturer.length()).trimmed();
    resourceData = dataPool->data(manufacturer, shortModel);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
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
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetAudioOutputConfigurations", result));
        return QnAudioTransmitterPtr();
    }
    else
    {
        NX_VERBOSE(this, makeSoapSuccessMessage(
            soapWrapper, __func__, "GetAudioOutputConfigurations"));
    }

    if (!response.Configurations.empty())
    {
        setAudioOutputConfigurationToken(QString::fromStdString(
            response.Configurations.front()->token));

        return std::make_shared<nx::vms::server::plugins::OnvifAudioTransmitter>(this);
    }

    NX_DEBUG(this, makeSoapNoParameterMessage(
        soapWrapper, "Configurations", __func__, "GetAudioOutputConfigurations"));

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
    const auto params = resourceData().value<TwoWayAudioParams>(ResourceDataKey::kTwoWayAudio);
    if (params.engine.toLower() == QString("onvif"))
    {
        result.reset(new nx::vms::server::plugins::OnvifAudioTransmitter(this));
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

QnPlOnvifResource::VideoEncoderCapabilities QnPlOnvifResource::findVideoEncoderCapabilities(
    UnderstandableVideoCodec encoding, Qn::StreamIndex streamIndex)
{
    const std::vector<VideoEncoderCapabilities>& list = (streamIndex == Qn::StreamIndex::primary)
        ? m_primaryStreamCapabilitiesList
        : m_secondaryStreamCapabilitiesList;

    if (list.empty())
        return VideoEncoderCapabilities();

    if (encoding == UnderstandableVideoCodec::NONE)
        return list.front();

    const auto it = std::find_if(list.cbegin(), list.cend(),
        [encoding](const VideoEncoderCapabilities& capabilities)
        {
            return capabilities.encoding == encoding;
        });
    if (it == list.cend())
    {
        NX_DEBUG(this, "Failed to find videoEncoderCapabilities for encoding = %1, streamIndex = %2",
            QString::fromStdString(VideoCodecToString(encoding)), toString(streamIndex));
        return VideoEncoderCapabilities();
    }

    return *it;
}

void QnPlOnvifResource::updateVideoEncoder(
    onvifXsd__VideoEncoderConfiguration& encoder,
    Qn::StreamIndex streamIndex,
    const QnLiveStreamParams& streamParams)
{
    QnLiveStreamParams params = streamParams;
    const auto resourceData = this->resourceData();

    auto useEncodingInterval = resourceData.value<bool>(
        ResourceDataKey::kControlFpsViaEncodingInterval);

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
        saveProperties();
    }
    const UnderstandableVideoCodec codec = VideoCodecFromString(streamParams.codec.toStdString());
    const auto capabilities = findVideoEncoderCapabilities(codec, streamIndex);

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
        NX_DEBUG(this, makeFailMessage("updateVideoEncoder: encoder.RateControl is not set"));
    }
    else
    {
        if (!useEncodingInterval)
        {
            encoder.RateControl->FrameRateLimit = params.fps;
        }
        else
        {
            int fpsBase = resourceData.value<int>(ResourceDataKey::kfpsBase);
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
        NX_DEBUG(this, makeFailMessage("updateVideoEncoder: encoder.Resolution is not set"));
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
    const QnResourceData resourceData = this->resourceData();

    auto useEncodingInterval = resourceData.value<bool>(
        ResourceDataKey::kControlFpsViaEncodingInterval);

    if (getProperty(QnMediaResource::customAspectRatioKey()).isEmpty())
    {
        bool forcedAR = resourceData.value<bool>(QString("forceArFromPrimaryStream"), false);
        if (forcedAR && streamParams.resolution.height() > 0)
        {
            QnAspectRatio ar(streamParams.resolution.width(), streamParams.resolution.height());
            setCustomAspectRatio(ar);
        }

        QString defaultAR = resourceData.value<QString>(QString("defaultAR"));
        QStringList parts = defaultAR.split(L'x');
        if (parts.size() == 2)
        {
            QnAspectRatio ar(parts[0].toInt(), parts[1].toInt());
            setCustomAspectRatio(ar);
        }
        saveProperties();
    }

    const UnderstandableVideoCodec codec = VideoCodecFromString(streamParams.codec.toStdString());
    const auto capabilities = findVideoEncoderCapabilities(codec, streamIndex);

    const AVCodecID codecId = QnAvCodecHelper::codecIdFromString(streamParams.codec);
    static const std::map<AVCodecID,std::string> kMedia2EncodingMap =
        { {AV_CODEC_ID_MJPEG, "JPEG"}, {AV_CODEC_ID_H264, "H264"}, {AV_CODEC_ID_HEVC, "H265"} };
    static const std::string defaultCodec = "JPEG";
    const auto it = kMedia2EncodingMap.find(codecId);

    encoder.Encoding = (it != kMedia2EncodingMap.end()) ? it->second : defaultCodec;

    if (!encoder.GovLength)
    {
        if (!m_govLength)
            m_govLength.reset(new int);
        encoder.GovLength = m_govLength.get();
    }
    *encoder.GovLength = qBound(capabilities.govMin, DEFAULT_IFRAME_DISTANCE, capabilities.govMax);

    if (codecId == AV_CODEC_ID_H264)
    {
        auto desiredH264Profile = resourceData.value<QString>(ResourceDataKey::kDesiredH264Profile);

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
    else if (codecId == AV_CODEC_ID_HEVC)
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

    QnLiveStreamParams params = streamParams; //< mutable copy of streamParams
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
            int fpsBase = resourceData.value<int>(ResourceDataKey::kfpsBase);
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
}

QnPlOnvifResource::VideoEncoderCapabilities QnPlOnvifResource::primaryVideoCapabilities() const
{
    QnMutexLocker lock(&m_mutex);
    return (!m_primaryStreamCapabilitiesList.empty())
        ? m_primaryStreamCapabilitiesList.front()
        : VideoEncoderCapabilities();
}

QnPlOnvifResource::VideoEncoderCapabilities QnPlOnvifResource::secondaryVideoCapabilities() const
{
    QnMutexLocker lock(&m_mutex);
    return (!m_secondaryStreamCapabilitiesList.empty())
        ? m_primaryStreamCapabilitiesList.front()
        : VideoEncoderCapabilities();
}

SoapTimeouts QnPlOnvifResource::onvifTimeouts() const
{
    if (commonModule()->isNeedToStop())
        return SoapTimeouts::minivalValue();
    else
        return SoapTimeouts(serverModule()->settings().onvifTimeouts());
}

#endif //ENABLE_ONVIF
