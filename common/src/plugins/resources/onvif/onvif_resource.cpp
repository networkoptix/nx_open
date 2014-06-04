
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

#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "onvif_helper.h"
#include "utils/common/synctime.h"
#include "utils/math/math.h"
#include "utils/common/timermanager.h"
#include "utils/common/systemerror.h"
#include "api/app_server_connection.h"
#include "soap/soapserver.h"
#include "soapStub.h"
#include "onvif_ptz_controller.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"

//!assumes that camera can only work in bistable mode (true for some (or all?) DW cameras)
#define SIMULATE_RELAY_PORT_MOMOSTABLE_MODE


const QString QnPlOnvifResource::MANUFACTURE(lit("OnvifDevice"));
static const float MAX_EPS = 0.01f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifResource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifResource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifResource::DEFAULT_IFRAME_DISTANCE = 20;
QString QnPlOnvifResource::MEDIA_URL_PARAM_NAME = QLatin1String("MediaUrl");
QString QnPlOnvifResource::ONVIF_URL_PARAM_NAME = QLatin1String("DeviceUrl");
QString QnPlOnvifResource::MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
QString QnPlOnvifResource::FORCED_AUDIO_SUPPORTED_PARAM_NAME = QLatin1String("forcedIsAudioSupported");
QString QnPlOnvifResource::AUDIO_SUPPORTED_PARAM_NAME = QLatin1String("isAudioSupported");
QString QnPlOnvifResource::DUAL_STREAMING_PARAM_NAME = QLatin1String("hasDualStreaming");
const float QnPlOnvifResource::QUALITY_COEF = 0.2f;
const int QnPlOnvifResource::MAX_AUDIO_BITRATE = 64; //kbps
const int QnPlOnvifResource::MAX_AUDIO_SAMPLERATE = 32; //khz
const int QnPlOnvifResource::ADVANCED_SETTINGS_VALID_TIME = 60; //60s
static const unsigned int DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT = 15;
//!if renew subscription exactly at termination time, camera can already terminate subscription, so have to do that a little bit earlier..
static const unsigned int RENEW_NOTIFICATION_FORWARDING_SECS = 5;
static const unsigned int MS_PER_SECOND = 1000;
static const unsigned int PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC = 1;

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

struct StrictBitrateInfo {
    const char* model;
    int minBitrate;
    int maxBitrate;
};

// TODO: #Elric #VASILENKO move out to JSON
// Strict bitrate range for specified cameras
StrictBitrateInfo strictBitrateList[] =
{
    { "DCS-7010L", 4096, 1024*16 },
    { "DCS-6010L", 0, 1024*2 }
};


//width > height is prefered
static bool resolutionGreaterThan(const QSize &s1, const QSize &s2)
{
    long long res1 = s1.width() * s1.height();
    long long res2 = s2.width() * s2.height();
    return res1 > res2? true: (res1 == res2 && s1.width() > s2.width()? true: false);
}

class VideoOptionsLocal
{
public:
    VideoOptionsLocal(): isH264(false), minQ(-1), maxQ(-1), frameRateMax(-1), govMin(-1), govMax(-1) {}
    VideoOptionsLocal(const QString& _id, const VideoOptionsResp& resp, bool isH264Allowed)
    {
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
            qSort(h264Profiles);

            if (resp.Options->H264->FrameRateRange)
                frameRateMax = resp.Options->H264->FrameRateRange->Max;
            if (resp.Options->H264->GovLengthRange) {
                govMin = resp.Options->H264->GovLengthRange->Min;
                govMax = resp.Options->H264->GovLengthRange->Max;
            }
        }
        else if (resp.Options->JPEG) {
            if (resp.Options->JPEG->FrameRateRange)
                frameRateMax = resp.Options->JPEG->FrameRateRange->Max;
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
    int frameRateMax;
    int govMin;
    int govMax;
};

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

    //if (square1Min != square2Min)
    //    return square1Min > square2Min;
    
    // analyse better resolution for secondary stream
    double coeff1, coeff2;
    QSize secondary1 = QnPlOnvifResource::findSecondaryResolution(max1Res, s1.resolutions, &coeff1);
    QSize secondary2 = QnPlOnvifResource::findSecondaryResolution(max2Res, s2.resolutions, &coeff2);
    if (qAbs(coeff1 - coeff2) > 1e-4) {

        if (!s1.isH264 && s2.isH264)
            coeff2 /= 1.3;
        else if (s1.isH264 && !s2.isH264)
            coeff1 /= 1.3;

        return coeff1 < coeff2; // less coeff is better
    }

    // if some option doesn't have H264 it "less"
    if (!s1.isH264 && s2.isH264)
        return false;
    else if (s1.isH264 && !s2.isH264)
        return true;

    return s1.id < s2.id; // sort by name
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

QnPlOnvifResource::QnPlOnvifResource()
:
    m_physicalParamsMutex(QMutex::Recursive),
    m_advSettingsLastUpdated(),
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
    m_needUpdateOnvifUrl(false),
    m_timeDrift(0),
    m_prevSoapCallResult(0),
    m_eventMonitorType( emtNone ),
    m_timerID( 0 ),
    m_renewSubscriptionTaskID(0),
    m_maxChannels(1),
    m_streamConfCounter(0),
    m_prevRequestSendClock(0)
{
    m_monotonicClock.start();
}

QnPlOnvifResource::~QnPlOnvifResource()
{
    {
        QMutexLocker lk( &m_subscriptionMutex );
        while( !m_triggerOutputTasks.empty() )
        {
            const quint64 timerID = m_triggerOutputTasks.begin()->first;
            const TriggerOutputTask outputTask = m_triggerOutputTasks.begin()->second;
            m_triggerOutputTasks.erase( m_triggerOutputTasks.begin() );
            lk.unlock();
            if( !outputTask.active )
            {
                //returning port to inactive state
                setRelayOutputStateNonSafe(
                    outputTask.outputID,
                    outputTask.active,
                    0 );
            }

            TimerManager::instance()->joinAndDeleteTimer( timerID );    //garantees that no onTimer(timerID) is running on return
            lk.relock();
        }
    }

    stopInputPortMonitoring();

    m_onvifAdditionalSettings.reset();
}

const QString QnPlOnvifResource::fetchMacAddress(const NetIfacesResp& response,
    const QString& senderIpAddress)
{
    QString someMacAddress;
    std::vector<class onvifXsd__NetworkInterface*> ifaces = response.NetworkInterfaces;

    for (uint i = 0; i < ifaces.size(); ++i)
    {
        onvifXsd__NetworkInterface* ifacePtr = ifaces[i];

        if (!ifacePtr->Info)
            continue;

        if (ifacePtr->Enabled && ifacePtr->IPv4->Enabled) {
            onvifXsd__IPv4Configuration* conf = ifacePtr->IPv4->Config;

            if (conf->DHCP && conf->FromDHCP) {
                //TODO: #vasilenko UTF unuse std::string
                if (senderIpAddress == QString::fromStdString(conf->FromDHCP->Address)) {
                    return QString::fromStdString(ifacePtr->Info->HwAddress).toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString::fromStdString(ifacePtr->Info->HwAddress);
                }
            }

            std::vector<class onvifXsd__PrefixedIPv4Address*> addresses = ifacePtr->IPv4->Config->Manual;
            std::vector<class onvifXsd__PrefixedIPv4Address*>::const_iterator addrPtrIter = addresses.begin();

            while (addrPtrIter != addresses.end()) {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;
                //TODO: #vasilenko UTF unuse std::string
                if (senderIpAddress == QString::fromStdString(addrPtr->Address)) {
                    return QString::fromStdString(ifacePtr->Info->HwAddress).toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString::fromStdString(ifacePtr->Info->HwAddress);
                }

                ++addrPtrIter;
            }
        }
    }

    return someMacAddress.toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
}

bool QnPlOnvifResource::setHostAddress(const QString &ip, QnDomain domain)
{
    //QnPhysicalCameraResource::se
    {
        QMutexLocker lock(&m_mutex);

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

    return QnPhysicalCameraResource::setHostAddress(ip, domain);
}

const QString QnPlOnvifResource::createOnvifEndpointUrl(const QString& ipAddress) {
    return QLatin1String(ONVIF_PROTOCOL_PREFIX) + ipAddress + QLatin1String(ONVIF_URL_SUFFIX);
}

bool QnPlOnvifResource::isResourceAccessible()
{
    return updateMACAddress();
}

QString QnPlOnvifResource::getDriverName() const
{
    return MANUFACTURE;
}

bool QnPlOnvifResource::hasDualStreaming() const
{
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlOnvifResource*>(this);
    this_casted->getParam(DUAL_STREAMING_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toBool();
}

const QSize QnPlOnvifResource::getVideoSourceSize() const
{
    QMutexLocker lock(&m_mutex);
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
    QMutexLocker lock(&m_mutex);
    return isPrimary ? m_primaryCodec : m_secondaryCodec;
}

void QnPlOnvifResource::setCodec(QnPlOnvifResource::CODECS c, bool isPrimary)
{
    QMutexLocker lock(&m_mutex);
    if (isPrimary)
        m_primaryCodec = c;
    else
        m_secondaryCodec = c;
}

QnPlOnvifResource::AUDIO_CODECS QnPlOnvifResource::getAudioCodec() const
{
    QMutexLocker lock(&m_mutex);
    return m_audioCodec;
}

void QnPlOnvifResource::setAudioCodec(QnPlOnvifResource::AUDIO_CODECS c)
{
    QMutexLocker lock(&m_mutex);
    m_audioCodec = c;
}

QnAbstractStreamDataProvider* QnPlOnvifResource::createLiveDataProvider()
{
    return new QnOnvifStreamReader(toSharedPointer());
}

void QnPlOnvifResource::setCroppingPhysical(QRect /*cropping*/)
{

}

CameraDiagnostics::Result QnPlOnvifResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    setCodec(H264, true);
    setCodec(H264, false);

    if (getDeviceOnvifUrl().isEmpty()) {
        qCritical() << "QnPlOnvifResource::initInternal: Can't do anything: ONVIF device url is absent. Id: " << getPhysicalId();
        return m_prevOnvifResultCode.errorCode != CameraDiagnostics::ErrorCode::noError
            ? m_prevOnvifResultCode
            : CameraDiagnostics::RequestFailedResult(QLatin1String("getDeviceOnvifUrl"), QString());
    }

    calcTimeDrift();

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    //if (getImagingUrl().isEmpty() || getMediaUrl().isEmpty() || getName().contains(QLatin1String("Unknown")) || getMAC().isNull() || m_needUpdateOnvifUrl)
    {
        //const CameraDiagnostics::Result result = fetchAndSetDeviceInformationPriv(false);
        CameraDiagnostics::Result result = getFullUrlInfo();
        if (!result)
            return result;
        
        if( getMediaUrl().isEmpty() )
        {
            qCritical() << "QnPlOnvifResource::initInternal: ONVIF media url is absent. Id: " << getPhysicalId();
            return CameraDiagnostics::CameraInvalidParams(lit("ONVIF media URL is not filled by camera"));
        }
        else
            m_needUpdateOnvifUrl = false;
    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    CameraDiagnostics::Result result = fetchAndSetVideoSource();
    if (!result)
        return result;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    fetchAndSetAudioSource();

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    result = fetchAndSetResourceOptions();
    if (!result) 
    {
        m_needUpdateOnvifUrl = true;
        return result;
    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    //if (getStatus() == QnResource::Offline || getStatus() == QnResource::Unauthorized)
    //    setStatus(QnResource::Online, true); // to avoid infinit status loop in this version

    //Additional camera settings
    fetchAndSetCameraSettings();

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    Qn::CameraCapabilities addFlags = Qn::NoCapabilities;
    if (m_primaryResolution.width() * m_primaryResolution.height() <= MAX_PRIMARY_RES_FOR_SOFT_MOTION)
        addFlags |= Qn::PrimaryStreamSoftMotionCapability;
    else if (!hasDualStreaming2())
        setMotionType(Qn::MT_NoMotion);

    
    if (addFlags != Qn::NoCapabilities)
        setCameraCapabilities(getCameraCapabilities() | addFlags);

    //registering onvif event handler
    std::vector<QnPlOnvifResource::RelayOutputInfo> relayOutputs;
    fetchRelayOutputs( &relayOutputs );
    if( !relayOutputs.empty() )
    {
        setCameraCapability( Qn::RelayOutputCapability, true );
        //TODO #ak it's not clear yet how to get input port list for sure (on DW cam getDigitalInputs returns nothing)
            //but all cameras i've seen have either both input & output or none
        setCameraCapability( Qn::RelayInputCapability, true );

        //resetting all ports states to inactive
        for( std::vector<QnPlOnvifResource::RelayOutputInfo>::size_type
            i = 0;
            i < relayOutputs.size();
            ++i )
        {
            setRelayOutputStateNonSafe( QString::fromStdString(relayOutputs[i].token), false, 0 );
        }
    }

#ifdef _DEBUG
    //startInputPortMonitoring();
#endif

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    fetchRelayInputInfo();

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    fetchPtzInfo();
    
    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    //detecting and saving selected resolutions
    CameraMediaStreams mediaStreams;
    mediaStreams.streams.push_back( CameraMediaStreamInfo( m_primaryResolution, m_primaryCodec == H264 ? CODEC_ID_H264 : CODEC_ID_MJPEG ) );
    if( m_secondaryResolution.width() > 0 )
        mediaStreams.streams.push_back( CameraMediaStreamInfo( m_secondaryResolution, m_secondaryCodec == H264 ? CODEC_ID_H264 : CODEC_ID_MJPEG ) );
    saveResolutionList( mediaStreams );

    saveParams();
    
    QnResourceData resourceData = qnCommon->dataPool()->data(toSharedPointer(this));
    bool forcedAR = resourceData.value<bool>(lit("forceArFromPrimaryStream"), false);
    if (forcedAR) 
    {
        qreal ar = m_primaryResolution.width() / (qreal) m_primaryResolution.height();
        
        QnKvPair kvPair;
        kvPair.setName(QnMediaResource::customAspectRatioKey());
        kvPair.setValue(QString::number(ar));

        QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
        QnKvPairList existPairs;
        if (conn->getKvPairs(existPairs, toSharedPointer(this)) == 0) 
        {
            bool aspectExists = false;
            foreach(const QnKvPair& existPair, existPairs)
            {
                if (existPair.name() == kvPair.name()) {
                    aspectExists = true;
                    break;
                }
            }
            if (!aspectExists) {
                setProperty(kvPair.name(), kvPair.value());
                conn->saveSync(getId(), kvPair);
            }
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

QSize QnPlOnvifResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolutionList.isEmpty()? EMPTY_RESOLUTION_PAIR: m_resolutionList.front();
}

QSize QnPlOnvifResource::getNearestResolutionForSecondary(const QSize& resolution, float aspectRatio) const
{
    QMutexLocker lock(&m_mutex);
    return getNearestResolution(resolution, aspectRatio, SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height(), m_secondaryResolutionList);
}

int QnPlOnvifResource::suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const
{
    return strictBitrate(QnPhysicalCameraResource::suggestBitrateKbps(quality, resolution, fps));
}

int QnPlOnvifResource::strictBitrate(int bitrate) const
{
    for (uint i = 0; i < sizeof(strictBitrateList) / sizeof(strictBitrateList[0]); ++i)
    {
        if (getModel() == QLatin1String(strictBitrateList[i].model))
            return qMin(strictBitrateList[i].maxBitrate, qMax(strictBitrateList[i].minBitrate, bitrate));
    }
    return bitrate;
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
    float currentAspect = getResolutionAspectRatio(primaryRes);
    int maxSquare = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();
    QSize result = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect, maxSquare, secondaryResList, matchCoeff);
    if (result == EMPTY_RESOLUTION_PAIR)
        result = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0, maxSquare, secondaryResList, matchCoeff); // try to get resolution ignoring aspect ration
    return result;
}

void QnPlOnvifResource::fetchAndSetPrimarySecondaryResolution()
{
    QMutexLocker lock(&m_mutex);

    if (m_resolutionList.isEmpty()) {
        return;
    }

    m_primaryResolution = m_resolutionList.front();
    checkPrimaryResolution(m_primaryResolution);

    m_secondaryResolution = findSecondaryResolution(m_primaryResolution, m_secondaryResolutionList);
    float currentAspect = getResolutionAspectRatio(m_primaryResolution);


    NX_LOG(QString(lit("ONVIF debug: got secondary resolution %1x%2 encoders for camera %3")).
                        arg(m_secondaryResolution.width()).arg(m_secondaryResolution.height()).arg(getHostAddress()), cl_logDEBUG1);


    if (m_secondaryResolution != EMPTY_RESOLUTION_PAIR) {
        Q_ASSERT(m_secondaryResolution.width() <= SECONDARY_STREAM_MAX_RESOLUTION.width() &&
            m_secondaryResolution.height() <= SECONDARY_STREAM_MAX_RESOLUTION.height());
        return;
    }

    double maxSquare = m_primaryResolution.width() * m_primaryResolution.height();

    foreach (QSize resolution, m_resolutionList) {
        float aspect = getResolutionAspectRatio(resolution);
        if (abs(aspect - currentAspect) < MAX_EPS) {
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

            Q_ASSERT(m_secondaryResolution.width() <= SECONDARY_STREAM_MAX_RESOLUTION.width() &&
                m_secondaryResolution.height() <= SECONDARY_STREAM_MAX_RESOLUTION.height());

            return;
        }
    }
}

QSize QnPlOnvifResource::getPrimaryResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_primaryResolution;
}

QSize QnPlOnvifResource::getSecondaryResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_secondaryResolution;
}

int QnPlOnvifResource::getPrimaryH264Profile() const
{
    QMutexLocker lock(&m_mutex);
    return m_primaryH264Profile;
}

int QnPlOnvifResource::getSecondaryH264Profile() const
{
    QMutexLocker lock(&m_mutex);
    return m_secondaryH264Profile;
}

void QnPlOnvifResource::setMaxFps(int f)
{
    setParam(MAX_FPS_PARAM_NAME, f, QnDomainDatabase);
}

int QnPlOnvifResource::getMaxFps() const
{
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlOnvifResource*>(this);

    if (!hasParam(MAX_FPS_PARAM_NAME))
    {
        return QnPhysicalCameraResource::getMaxFps();
    }

    this_casted->getParam(MAX_FPS_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toInt();
}

const QString QnPlOnvifResource::getPrimaryVideoEncoderId() const
{
    QMutexLocker lock(&m_mutex);
    return m_primaryVideoEncoderId;
}

const QString QnPlOnvifResource::getSecondaryVideoEncoderId() const
{
    QMutexLocker lock(&m_mutex);
    return m_secondaryVideoEncoderId;
}

const QString QnPlOnvifResource::getAudioEncoderId() const
{
    QMutexLocker lock(&m_mutex);
    return m_audioEncoderId;
}

const QString QnPlOnvifResource::getVideoSourceId() const
{
    QMutexLocker lock(&m_mutex);
    return m_videoSourceId;
}

const QString QnPlOnvifResource::getAudioSourceId() const
{
    QMutexLocker lock(&m_mutex);
    return m_audioSourceId;
}

QString QnPlOnvifResource::getDeviceOnvifUrl() const 
{ 
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlOnvifResource*>(this);
    this_casted->getParam(ONVIF_URL_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toString();
}

void QnPlOnvifResource::setDeviceOnvifUrl(const QString& src) 
{ 
    setParam(ONVIF_URL_PARAM_NAME, src, QnDomainDatabase);
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
    CameraDiagnostics::Result result =  readDeviceInformation(getDeviceOnvifUrl(), getAuth(), m_timeDrift, &extInfo);
    if (result) {
        if (getName().isEmpty())
            setName(extInfo.name);
        if (getModel().isEmpty())
            setModel(extInfo.model);
        if (getFirmware().isEmpty())
            setFirmware(extInfo.firmware);
        if (getMAC().isNull())
            setMAC(QnMacAddress(extInfo.mac));
        if (getPhysicalId().isEmpty())
            setPhysicalId(extInfo.hardwareId);
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

    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes != SOAP_OK) 
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
            << soapWrapper.getEndpointUrl() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();

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
    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    //Trying to get MAC
    NetIfacesReq requestIfList;
    NetIfacesResp responseIfList;

    soapRes = soapWrapper.getNetworkInterfaces(requestIfList, responseIfList); // this request is optional
    if( soapRes != SOAP_OK )
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch MAC address. Reason: SOAP to endpoint "
            << onvifUrl << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
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
    if( !notification.Message.__any )
    {
        NX_LOG( lit("Received notification with empty message. Ignoring..."), cl_logDEBUG2 );
        return;
    }

    if( !notification.oasisWsnB2__Topic ||
        !notification.oasisWsnB2__Topic->__item )
    {
        NX_LOG( lit("Received notification with no topic specified. Ignoring..."), cl_logDEBUG2 );
        return;
    }

    if( std::strstr(notification.oasisWsnB2__Topic->__item, "Trigger/Relay") == nullptr &&
        std::strstr(notification.oasisWsnB2__Topic->__item, "IO/Port") == nullptr )
    {
        NX_LOG( lit("Received notification with unknown topic: %1. Ignoring...").
            arg(QLatin1String(notification.oasisWsnB2__Topic->__item)), cl_logDEBUG2 );
        return;
    }

    //parsing Message
    QXmlSimpleReader reader;
    NotificationMessageParseHandler handler;
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

    //checking that there is single source and this source is a relay port
    std::list<NotificationMessageParseHandler::SimpleItem>::const_iterator portSourceIter = handler.source.end();
    for( std::list<NotificationMessageParseHandler::SimpleItem>::const_iterator
        it = handler.source.begin();
        it != handler.source.end();
        ++it )
    {
        if( it->name == QLatin1String("port") || 
            it->name == QLatin1String("RelayToken") || 
            it->name == QLatin1String("RelayInputToken") )
        {
            portSourceIter = it;
            break;
        }
    }

    if( portSourceIter == handler.source.end()  //source is not port
        || (handler.data.name != QLatin1String("LogicalState") &&
            handler.data.name != QLatin1String("state")) )
    {
        return;
    }

    //saving port state
    const bool newPortState = (handler.data.value == QLatin1String("true")) || (handler.data.value == QLatin1String("active"));
    bool& currentPortState = m_relayInputStates[portSourceIter->value];
    if( currentPortState != newPortState )
    {
        currentPortState = newPortState;
        emit cameraInput(
            toSharedPointer(),
            portSourceIter->value,
            newPortState,
            QDateTime::currentMSecsSinceEpoch() );  //it is not absolutely correct, but better than de-synchronized timestamp from camera
            //handler.utcTime.toMSecsSinceEpoch() );
    }
}

void QnPlOnvifResource::onTimer( const quint64& timerID )
{
    //TODO refactoring needed. Processed task type should be made more clear (enum would be appropriate)

    TriggerOutputTask triggerOutputTask;
    bool triggeringOutput = false;
    {
        QMutexLocker lk( &m_subscriptionMutex );
        std::map<quint64, TriggerOutputTask>::iterator it = m_triggerOutputTasks.find( timerID );
        if( it != m_triggerOutputTasks.end() )
        {
            triggerOutputTask = it->second;
            triggeringOutput = true;
            m_triggerOutputTasks.erase( it );
        }
    }

    if( triggeringOutput )
    {
        setRelayOutputStateNonSafe(
            triggerOutputTask.outputID,
            triggerOutputTask.active,
            triggerOutputTask.autoResetTimeoutMS );
        return;
    }

    if( timerID == m_renewSubscriptionTaskID )
    {
        onRenewSubscriptionTimer();
        return;
    }

    switch( m_eventMonitorType )
    {
        case emtNotification:
            onRenewSubscriptionTimer();
            break;

        case emtPullPoint:
            pullMessages();
            break;

        default:
            break;
    }

    //setRelayOutputState( QString::fromStdString(m_relayOutputInfo.front().token), true, 5 );
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetResourceOptions()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);

    CameraDiagnostics::Result result = fetchAndSetVideoEncoderOptions(soapWrapper);
    if (!result)
        return result;

    result = updateResourceCapabilities();
    if (!result)
        return result;

    //All VideoEncoder options are set, so we can calculate resolutions for the streams
    fetchAndSetPrimarySecondaryResolution();

    //Before invoking <fetchAndSetHasDualStreaming> Primary and Secondary Resolutions MUST be set
    fetchAndSetDualStreaming(soapWrapper);

    if (fetchAndSetAudioEncoder(soapWrapper) && fetchAndSetAudioEncoderOptions(soapWrapper))
        setParam(AUDIO_SUPPORTED_PARAM_NAME, 1, QnDomainDatabase);
    else
        setParam(AUDIO_SUPPORTED_PARAM_NAME, 0, QnDomainDatabase);

    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::updateSecondaryResolutionList(const VideoOptionsLocal& opts)
{
    QMutexLocker lock(&m_mutex);
    m_secondaryResolutionList = opts.resolutions;
    qSort(m_secondaryResolutionList.begin(), m_secondaryResolutionList.end(), resolutionGreaterThan);
}

void QnPlOnvifResource::setVideoEncoderOptions(const VideoOptionsLocal& opts) {
    if (opts.minQ != -1) {
        setMinMaxQuality(opts.minQ, opts.maxQ);

        NX_LOG(QString(lit("ONVIF quality range [%1, %2]")).arg(m_minQuality).arg(m_maxQuality), cl_logDEBUG1);

    } else {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: camera didn't return quality range. UniqueId: " << getUniqueId();
    }

    if (opts.isH264) 
        setVideoEncoderOptionsH264(opts);
    else 
        setVideoEncoderOptionsJpeg(opts);
}

void QnPlOnvifResource::setVideoEncoderOptionsH264(const VideoOptionsLocal& opts) 
{

    if (opts.frameRateMax > 0)
        setMaxFps(opts.frameRateMax);
    else 
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Max FPS. UniqueId: " << getUniqueId();

    if (opts.govMin >= 0) 
    {
        QMutexLocker lock(&m_mutex);
        m_iframeDistance = DEFAULT_IFRAME_DISTANCE <= opts.govMin ? opts.govMin : (DEFAULT_IFRAME_DISTANCE >= opts.govMax ? opts.govMax: DEFAULT_IFRAME_DISTANCE);
        NX_LOG(QString(lit("ONVIF iframe distance: %1")).arg(m_iframeDistance), cl_logDEBUG1);
    } 
    else {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Iframe Distance. UniqueId: " << getUniqueId();
    }

    if (opts.resolutions.isEmpty()) {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Resolutions. UniqueId: " << getUniqueId();
    } 
    else 
    {
        QMutexLocker lock(&m_mutex);
        m_resolutionList = opts.resolutions;
        qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);

    }

    QMutexLocker lock(&m_mutex);

    //Printing fetched resolutions
    if (cl_log.logLevel() > cl_logDEBUG1) {
        NX_LOG(QString(lit("ONVIF resolutions:")), cl_logDEBUG1);
        foreach (QSize resolution, m_resolutionList) {
            NX_LOG(QString(lit("%1x%2")).arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
        }
    }
}

void QnPlOnvifResource::setVideoEncoderOptionsJpeg(const VideoOptionsLocal& opts)
{
    if (opts.frameRateMax > 0)
        setMaxFps(opts.frameRateMax);
    else 
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Max FPS. UniqueId: " << getUniqueId();

    if (opts.resolutions.isEmpty())
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Resolutions. UniqueId: " << getUniqueId();
    } 
    else 
    {
        QMutexLocker lock(&m_mutex);
        m_resolutionList = opts.resolutions;
        qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);
    }

    QMutexLocker lock(&m_mutex);
    //Printing fetched resolutions
    if (cl_log.logLevel() > cl_logDEBUG1) {
        NX_LOG(QString(lit("ONVIF resolutions:")), cl_logDEBUG1);
        foreach (QSize resolution, m_resolutionList) {
            NX_LOG(QString(lit("%1x%2")).arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
        }
    }
}

int QnPlOnvifResource::innerQualityToOnvif(Qn::StreamQuality quality) const
{
    if (quality > Qn::QualityHighest) {
        qWarning() << "QnPlOnvifResource::innerQualityToOnvif: got unexpected quality (too big): " << quality;
        return m_maxQuality;
    }
    if (quality < Qn::QualityLowest) {
        qWarning() << "QnPlOnvifResource::innerQualityToOnvif: got unexpected quality (too small): " << quality;
        return m_minQuality;
    }

    NX_LOG(QString(lit("QnPlOnvifResource::innerQualityToOnvif: in quality = %1, out qualty = %2, minOnvifQuality = %3, maxOnvifQuality = %4"))
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
    return m_timeDrift;
}

void QnPlOnvifResource::calcTimeDrift()
{
    m_timeDrift = calcTimeDrift(getDeviceOnvifUrl());
}

int QnPlOnvifResource::calcTimeDrift(const QString& deviceUrl)
{
    DeviceSoapWrapper soapWrapper(deviceUrl.toStdString(), QString(), QString(), 0);

    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = soapWrapper.GetSystemDateAndTime(request, response);

    if (soapRes == SOAP_OK && response.SystemDateAndTime && response.SystemDateAndTime->UTCDateTime)
    {
        onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
        onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;

        QDateTime datetime(QDate(date->Year, date->Month, date->Day), QTime(time->Hour, time->Minute, time->Second), Qt::UTC);
        int drift = datetime.toMSecsSinceEpoch()/MS_PER_SECOND - QDateTime::currentMSecsSinceEpoch()/MS_PER_SECOND;
        return drift;
    }
    return 0;
}

QString QnPlOnvifResource::getMediaUrl() const 
{ 
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlOnvifResource*>(this);
    this_casted->getParam(MEDIA_URL_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toString();
}

void QnPlOnvifResource::setMediaUrl(const QString& src) 
{
    setParam(MEDIA_URL_PARAM_NAME, src, QnDomainDatabase);
}

QString QnPlOnvifResource::getImagingUrl() const 
{ 
    QMutexLocker lock(&m_mutex);
    return m_imagingUrl;
}

void QnPlOnvifResource::setImagingUrl(const QString& src) 
{
    QMutexLocker lock(&m_mutex);
    m_imagingUrl = src;
}

QString QnPlOnvifResource::getVideoSourceToken() const {
    QMutexLocker lock(&m_mutex);
    return m_videoSourceToken;
}

void QnPlOnvifResource::setVideoSourceToken(const QString &src) {
    QMutexLocker lock(&m_mutex);
    m_videoSourceToken = src;
}

QString QnPlOnvifResource::getPtzUrl() const
{
    QMutexLocker lock(&m_mutex);
    return m_ptzUrl;
}

void QnPlOnvifResource::setPtzUrl(const QString& src) 
{
    QMutexLocker lock(&m_mutex);
    m_ptzUrl = src;
}

QString QnPlOnvifResource::getPtzConfigurationToken() const {
    QMutexLocker lock(&m_mutex);
    return m_ptzConfigurationToken;
}

void QnPlOnvifResource::setPtzConfigurationToken(const QString &src) {
    QMutexLocker lock(&m_mutex);
    m_ptzConfigurationToken = src;
}

QString QnPlOnvifResource::getPtzProfileToken() const
{
    QMutexLocker lock(&m_mutex);
    return m_ptzProfileToken;
}

void QnPlOnvifResource::setPtzProfileToken(const QString& src) 
{
    QMutexLocker lock(&m_mutex);
    m_ptzProfileToken = src;
}

void QnPlOnvifResource::setMinMaxQuality(int min, int max)
{
    int netoptixDelta = Qn::QualityHighest - Qn::QualityLowest;
    int onvifDelta = max - min;

    if (netoptixDelta < 0 || onvifDelta < 0) {
        qWarning() << "QnPlOnvifResource::setMinMaxQuality: incorrect values: min > max: onvif ["
                   << min << ", " << max << "] netoptix [" << Qn::QualityLowest << ", " << Qn::QualityHighest << "]";
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

    if (getModel() != onvifR->getModel()) {
        setModel(onvifR->getModel());
        result = true;
    }
    if (getVendor() != onvifR->getVendor()) {
        setVendor(onvifR->getVendor());
        result = true;
    }
    if (getMAC() != onvifR->getMAC()) {
        setMAC(onvifR->getMAC());
        result = true;
    }

    /*
    QString onvifUrlSource = onvifR->getDeviceOnvifUrl();
    QString mediaUrlSource = onvifR->getMediaUrl();


    if (onvifUrlSource.size() != 0 && QUrl(onvifUrlSource).host().size() != 0 && getDeviceOnvifUrl() != onvifUrlSource)
    {
        setDeviceOnvifUrl(onvifUrlSource);
        result = true;
    }

    if (mediaUrlSource.size() != 0 && QUrl(mediaUrlSource).host().size() != 0 && getMediaUrl() != mediaUrlSource)
    {
        setMediaUrl(mediaUrlSource);
        result = true;
    }

    if (getDeviceOnvifUrl().size()!=0 && QUrl(getDeviceOnvifUrl()).host().size()==0)
    {
        // trying to introduce fix for dw cam 
        QString temp = getDeviceOnvifUrl();

        QUrl newUrl(getDeviceOnvifUrl());
        newUrl.setHost(getHostAddress());
        setDeviceOnvifUrl(newUrl.toString());
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << getDeviceOnvifUrl();
        result = true;
    }

    if (getMediaUrl().size() != 0 && QUrl(getMediaUrl()).host().size()==0)
    {
        // trying to introduce fix for dw cam 
        QString temp = getMediaUrl();
        QUrl newUrl(getMediaUrl());
        newUrl.setHost(getHostAddress());
        setMediaUrl(newUrl.toString());
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << getMediaUrl();
        result = true;
    }
    */

    return result;
}

static QString getRelayOutpuToken( const QnPlOnvifResource::RelayOutputInfo& relayInfo )
{
    return QString::fromStdString( relayInfo.token );
}

//!Implementation of QnSecurityCamResource::getRelayOutputList
QStringList QnPlOnvifResource::getRelayOutputList() const
{
    QStringList idList;
    std::transform(
        m_relayOutputInfo.begin(),
        m_relayOutputInfo.end(),
        std::back_inserter(idList),
        getRelayOutpuToken );
    return idList;
}

QStringList QnPlOnvifResource::getInputPortList() const
{
    //TODO/IMPL
    return QStringList();
}

bool QnPlOnvifResource::fetchRelayInputInfo()
{
    if( m_deviceIOUrl.empty() )
        return false;

    const QAuthenticator& auth = getAuth();
    DeviceIOWrapper soapWrapper(
        m_deviceIOUrl,
        auth.user(),
        auth.password(),
        m_timeDrift );

    _onvifDeviceIO__GetDigitalInputs request;
    _onvifDeviceIO__GetDigitalInputsResponse response;
    m_prevSoapCallResult = soapWrapper.getDigitalInputs( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to get relay digital input list. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
        return true;
    }

    return true;
}

bool QnPlOnvifResource::fetchPtzInfo() {
    if(m_ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(getAuth());
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
    QMutexLocker lk( &m_subscriptionMutex );

    const quint64 timerID = TimerManager::instance()->addTimer( this, 0 );
    m_triggerOutputTasks.insert( std::make_pair( timerID, TriggerOutputTask( outputID, active, autoResetTimeoutMS ) ) );
    return true;
}

int QnPlOnvifResource::getH264StreamProfile(const VideoOptionsLocal& videoOptionsLocal)
{
    if (videoOptionsLocal.h264Profiles.isEmpty())
        return -1;
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

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper)
{
    VideoConfigsReq confRequest;
    VideoConfigsResp confResponse;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse); // get encoder list
    if (soapRes != SOAP_OK) {
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't get list of video encoders from camera (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << "). GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurations"), soapWrapper.getLastError());
    }

    QString login = soapWrapper.getLogin();
    QString password = soapWrapper.getPassword();
    std::string endpoint = soapWrapper.getEndpointUrl().toStdString();

    int confRangeStart = 0;
    int confRangeEnd = (int) confResponse.Configurations.size();
    if (m_maxChannels > 1)
    {
        // determine amount encoder configurations per each video source
        confRangeStart = confRangeEnd/m_maxChannels * getChannel();
        confRangeEnd = confRangeStart + confRangeEnd/m_maxChannels;

        if (confRangeEnd > (int) confResponse.Configurations.size()) {
            qWarning() << "invalid channel number " << getChannel()+1 << "for camera" << getHostAddress() << "max channels=" << m_maxChannels;
            return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurationOptions"), soapWrapper.getLastError());
        }
    }

    QList<VideoOptionsLocal> optionsList;
    for (int confNum = confRangeStart; confNum < confRangeEnd; ++confNum)
    {
        onvifXsd__VideoEncoderConfiguration* configuration = confResponse.Configurations[confNum];
        if (!configuration)
            continue;

        int retryCount = getMaxOnvifRequestTries();
        soapRes = SOAP_ERR;

        for (;soapRes != SOAP_OK && retryCount >= 0; --retryCount)
        {
            VideoOptionsReq optRequest;
            VideoOptionsResp optResp;
            optRequest.ConfigurationToken = &configuration->token;
            optRequest.ProfileToken = NULL;

            MediaSoapWrapper soapWrapper(endpoint, login, password, m_timeDrift);

            soapRes = soapWrapper.getVideoEncoderConfigurationOptions(optRequest, optResp); // get options per encoder
            if (soapRes != SOAP_OK || !optResp.Options) {

                qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't receive options (or data is empty) for video encoder '" 
                    << QString::fromStdString(*(optRequest.ConfigurationToken)) << "' from camera (URL: "  << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
                    << "). Root cause: SOAP request failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        
                qWarning() << "camera" << soapWrapper.getEndpointUrl() << "got soap error for configuration" << configuration->Name.c_str() << "skip configuration";
            continue;
        }

            if (optResp.Options->H264 || optResp.Options->JPEG)
                optionsList << VideoOptionsLocal(QString::fromStdString(configuration->token), optResp, isH264Allowed());
            else
                qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: video encoder '" << optRequest.ConfigurationToken->c_str()
                    << "' contains no data for H264/JPEG (URL: "  << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << ")." << "Ignoring and use default codec list";
        }
        if (soapRes != SOAP_OK)
            return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurationOptions"), soapWrapper.getLastError());
    }

    qSort(optionsList.begin(), optionsList.end(), videoOptsGreaterThan);

    if (optionsList.isEmpty())
    {
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: all video options are empty. (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << ").";
        return CameraDiagnostics::RequestFailedResult(QLatin1String("fetchAndSetVideoEncoderOptions"), QLatin1String("no video options"));
    }

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
    if (m_maxChannels == 1 && !isCameraControlDisabled())
        checkMaxFps(confResponse, optionsList[0].id);

    m_mutex.lock();
    m_primaryVideoEncoderId = optionsList[0].id;
    m_secondaryResolutionList = m_resolutionList;
    m_mutex.unlock();

    NX_LOG(QString(lit("ONVIF debug: got %1 encoders for camera %2")).arg(optionsList.size()).arg(getHostAddress()), cl_logDEBUG1);

    bool dualStreamingAllowed = optionsList.size() >= 2;
    if (dualStreamingAllowed)
    {
        int secondaryIndex = getSecondaryIndex(optionsList);
        QMutexLocker lock(&m_mutex);

        m_secondaryVideoEncoderId = optionsList[secondaryIndex].id;
        if (optionsList[secondaryIndex].isH264) {
            m_secondaryH264Profile = getH264StreamProfile(optionsList[secondaryIndex]);
                setCodec(H264, false);
                NX_LOG(QString(lit("use H264 codec for secondary stream. camera=%1")).arg(getHostAddress()), cl_logDEBUG1);
            }
            else {
                setCodec(JPEG, false);
                NX_LOG(QString(lit("use JPEG codec for secondary stream. camera=%1")).arg(getHostAddress()), cl_logDEBUG1);
            }
        updateSecondaryResolutionList(optionsList[secondaryIndex]);
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::fetchAndSetDualStreaming(MediaSoapWrapper& /*soapWrapper*/)
{
    QMutexLocker lock(&m_mutex);

    bool dualStreaming = m_secondaryResolution != EMPTY_RESOLUTION_PAIR && !m_secondaryVideoEncoderId.isEmpty();
    if (dualStreaming) {
        NX_LOG(QString(lit("ONVIF debug: enable dualstreaming for camera %1")).arg(getHostAddress()), cl_logDEBUG1);
    }
    else {
        QString reason = m_secondaryResolution == EMPTY_RESOLUTION_PAIR ? QLatin1String("no secondary resolution") : QLatin1String("no secondary encoder");
        NX_LOG(QString(lit("ONVIF debug: disable dualstreaming for camera %1 reason: %2")).arg(getHostAddress()).arg(reason), cl_logDEBUG1);
    }

    setParam(DUAL_STREAMING_PARAM_NAME, dualStreaming ? 1 : 0, QnDomainDatabase);
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

        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
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
                    qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: got unknown codec type: " 
                        << curOpts->Encoding << " (URL: " << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
                        << "). Root cause: SOAP request failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
                    break;
            }
        }

        ++it;
    }

    if (!options) {

        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return data for G711, G726 or ACC (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
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
    else
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return Bitrate List ( UniqueId: " 
            << getUniqueId() << ").";
    }

    int sampleRate = 0;
    if (options.SampleRateList)
    {
        sampleRate = findClosestRateFloor(options.SampleRateList->Items, MAX_AUDIO_SAMPLERATE);
    }
    else
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoderOptions: camera didn't return Samplerate List ( UniqueId: " 
            << getUniqueId() << ").";
    }

    {
        QMutexLocker lock(&m_mutex);
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
    QMutexLocker lock(&m_mutex);

    if (!m_videoSourceSize.isValid()) {
        return CameraDiagnostics::NoErrorResult();
    }

    QList<QSize>::iterator it = m_resolutionList.begin();
    while (it != m_resolutionList.end())
    {
        if (it->width() > m_videoSourceSize.width() || it->height() > m_videoSourceSize.height())
            it = m_resolutionList.erase(it);
        else
            return CameraDiagnostics::NoErrorResult();
        }

    return CameraDiagnostics::NoErrorResult();
}

int QnPlOnvifResource::getGovLength() const
{
    QMutexLocker lock(&m_mutex);

    return m_iframeDistance;
}

bool QnPlOnvifResource::fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper)
{
    AudioConfigsReq request;
    AudioConfigsResp response;

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoder: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;

    }

    if (response.Configurations.empty()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioEncoder: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;
    } else {
        if ((int)response.Configurations.size() > getChannel())
        {
            onvifXsd__AudioEncoderConfiguration* conf = response.Configurations.at(getChannel());
        if (conf) {
            QMutexLocker lock(&m_mutex);
            //TODO: #vasilenko UTF unuse std::string
            m_audioEncoderId = QString::fromStdString(conf->token);
        }
    }
        else {
            qWarning() << "Can't find appropriate audio encoder. url=" << getUrl();
            return false;
        }
    }

    return true;
}

void QnPlOnvifResource::updateVideoSource(VideoSource* source, const QRect& maxRect) const
{
    //One name for primary and secondary
    //source.Name = NETOPTIX_PRIMARY_NAME;

    if (!source->Bounds) {
        qWarning() << "QnOnvifStreamReader::updateVideoSource: rectangle object is NULL. UniqueId: " << getUniqueId();
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
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), getTimeDrift());

    SetVideoSrcConfigReq request;
    SetVideoSrcConfigResp response;
    request.Configuration = source;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::setVideoSourceConfiguration: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();

        if (soapWrapper.isNotAuthenticated())
            setStatus(QnResource::Unauthorized);

        return CameraDiagnostics::NoErrorResult(); // ignore error because of some cameras is not ONVIF profile S compatible and doesn't support this request
        //return CameraDiagnostics::RequestFailedResult(QLatin1String("setVideoSourceConfiguration"), soapWrapper.getLastError());
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlOnvifResource::detectVideoSourceCount()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);

    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
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
        if ( (int)confResponse.Configurations.size() < m_maxChannels)
            m_maxChannels = confResponse.Configurations.size();
    }

    return true;
}

CameraDiagnostics::Result QnPlOnvifResource::fetchVideoSourceToken()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);

    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        if (soapWrapper.isNotAuthenticated())
        {
            setStatus(QnResource::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult( getMediaUrl() );
        }
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSources"), soapWrapper.getLastError());

    }

    m_maxChannels = (int) response.VideoSources.size();

    if (m_maxChannels <= getChannel()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSources"), QLatin1String("missing video source configuration (1)"));
    } 

    onvifXsd__VideoSource* conf = response.VideoSources.at(getChannel());

    if (!conf)
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSources"), QLatin1String("missing video source configuration (2)"));

    QMutexLocker lock(&m_mutex);
    m_videoSourceToken = QString::fromStdString(conf->token);
    //m_videoSourceSize = QSize(conf->Resolution->Width, conf->Resolution->Height);

    if (m_maxChannels > 1)
    {
        VideoConfigsReq confRequest;
        VideoConfigsResp confResponse;
        soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse); // get encoder list
        if (soapRes != SOAP_OK)
            return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfigurations"), soapWrapper.getLastError());

        if ( (int)confResponse.Configurations.size() < m_maxChannels)
            m_maxChannels = confResponse.Configurations.size();
    }

    return CameraDiagnostics::NoErrorResult();
}

QRect QnPlOnvifResource::getVideoSourceMaxSize(const QString& configToken)
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    VideoSrcOptionsReq request;
    std::string token = configToken.toStdString();
    request.ConfigurationToken = &token;
    request.ProfileToken = NULL;

    VideoSrcOptionsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurationOptions(request, response);
    if (soapRes != SOAP_OK || !response.Options) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSourceOptions: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return QRect();
    }
    onvifXsd__IntRectangleRange* br = response.Options->BoundsRange;
    return QRect(qMax(0, br->XRange->Min), qMax(0, br->YRange->Min), br->WidthRange->Max, br->HeightRange->Max);
}

CameraDiagnostics::Result QnPlOnvifResource::fetchAndSetVideoSource()
{
    CameraDiagnostics::Result result = fetchVideoSourceToken();
    if (!result)
        return result;

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    VideoSrcConfigsReq request;
    VideoSrcConfigsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoSourceConfigurations"), soapWrapper.getLastError());

    }

    if (m_appStopping)
        return CameraDiagnostics::ServerTerminatedResult();

    std::string srcToken = m_videoSourceToken.toStdString();
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        onvifXsd__VideoSourceConfiguration* conf = response.Configurations.at(i);
        if (!conf || conf->SourceToken != srcToken)
            continue;

        {
            QMutexLocker lock(&m_mutex);
            m_videoSourceId = QString::fromStdString(conf->token);
        }

        QRect currentRect(conf->Bounds->x, conf->Bounds->y, conf->Bounds->width, conf->Bounds->height);
        QRect maxRect = getVideoSourceMaxSize(QString::fromStdString(conf->token));
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
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    AudioSrcConfigsReq request;
    AudioSrcConfigsResp response;

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getAudioSourceConfigurations"), soapWrapper.getLastError());

    }

    if ((int)response.Configurations.size() <= getChannel()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getAudioSourceConfigurations"), QLatin1String("missing channel configuration (1)"));
    } else {
        onvifXsd__AudioSourceConfiguration* conf = response.Configurations.at(getChannel());
        if (conf) {
            QMutexLocker lock(&m_mutex);
            //TODO: #vasilenko UTF unuse std::string
            m_audioSourceId = QString::fromStdString(conf->token);
            return CameraDiagnostics::NoErrorResult();
        }
    }

    return CameraDiagnostics::RequestFailedResult(QLatin1String("getAudioSourceConfigurations"), QLatin1String("missing channel configuration (2)"));
}

QnConstResourceAudioLayoutPtr QnPlOnvifResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
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

bool QnPlOnvifResource::getParamPhysical(const QnParam &param, QVariant &val)
{
    m_prevOnvifResultCode = CameraDiagnostics::NoErrorResult();

    QMutexLocker lock(&m_physicalParamsMutex);
    if (!m_onvifAdditionalSettings) {
        m_prevOnvifResultCode = CameraDiagnostics::UnknownErrorResult();
        return false;
    }

    CameraSettings& settings = m_onvifAdditionalSettings->getCameraSettings();
    CameraSettings::Iterator it = settings.find(param.name());

    if (it == settings.end()) {
        //This is the case when camera doesn't contain Media Service, but the client doesn't know about it and
        //sends request for param from this service. Can't return false in this case, because our framework stops
        //fetching physical params after first failed.
        return true;
    }

    //Caching camera values during ADVANCED_SETTINGS_VALID_TIME to avoid multiple excessive 'get' requests 
    //to camera. All values can be get by one request, but our framework do getParamPhysical for every single param.
    QDateTime currTime = QDateTime::currentDateTime();
    if (m_advSettingsLastUpdated.isNull() || m_advSettingsLastUpdated.secsTo(currTime) > ADVANCED_SETTINGS_VALID_TIME) {
        m_prevOnvifResultCode = m_onvifAdditionalSettings->makeGetRequest();
        if( m_prevOnvifResultCode.errorCode != CameraDiagnostics::ErrorCode::noError )
            return false;
        m_advSettingsLastUpdated = currTime;
    }

    if (it.value().getFromCamera(*m_onvifAdditionalSettings)) {
        val.setValue(it.value().serializeToStr());
        return true;
    }

    //If server can't get value from camera, it will be marked in "QVariant &val" as empty m_current param
    //Completely empty "QVariant &val" means enabled setting with no value (ex: Settings tree element or button)
    //Can't return false in this case, because our framework stops fetching physical params after first failed.
    return true;
}

bool QnPlOnvifResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    QMutexLocker lock(&m_physicalParamsMutex);
    if (!m_onvifAdditionalSettings)
        return false;

    CameraSetting tmp;
    tmp.deserializeFromStr(val.toString());

    CameraSettings& settings = m_onvifAdditionalSettings->getCameraSettings();
    CameraSettings::Iterator it = settings.find(param.name());

    if (it == settings.end())
    {
        //Buttons are not contained in CameraSettings
        if (tmp.getType() != CameraSetting::Button) {
            return false;
        }

        //For Button - only operation object is required
        QHash<QString, QSharedPointer<OnvifCameraSettingOperationAbstract> >::ConstIterator opIt =
            OnvifCameraSettingOperationAbstract::operations.find(param.name());

        if (opIt == OnvifCameraSettingOperationAbstract::operations.end()) {
            return false;
        }

        return opIt.value()->set(tmp, *m_onvifAdditionalSettings);
    }

    CameraSettingValue oldVal = it.value().getCurrent();
    it.value().setCurrent(tmp.getCurrent());


    if (!it.value().setToCamera(*m_onvifAdditionalSettings)) {
        it.value().setCurrent(oldVal);
        return false;
    }

    return true;
}

void QnPlOnvifResource::fetchAndSetCameraSettings()
{
    QString imagingUrl = getImagingUrl();
    if (imagingUrl.isEmpty()) {
        NX_LOG(QString(lit("QnPlOnvifResource::fetchAndSetCameraSettings: imaging service is absent on device (URL: %1, UniqId: %2"))
            .arg(getDeviceOnvifUrl()).arg(getUniqueId()), cl_logDEBUG1);
    }

    QAuthenticator auth(getAuth());
    std::unique_ptr<OnvifCameraSettingsResp> settings( new OnvifCameraSettingsResp(
        getDeviceOnvifUrl().toLatin1().data(), imagingUrl.toLatin1().data(),
        auth.user(), auth.password(), m_videoSourceToken.toStdString(), getUniqueId(), m_timeDrift) );

    if (!imagingUrl.isEmpty()) {
        settings->makeGetRequest();
    }

    if (m_appStopping)
        return;

    OnvifCameraSettingReader reader(*settings);

    reader.read() && reader.proceed();

    CameraSettings& onvifSettings = settings->getCameraSettings();
    CameraSettings::ConstIterator it = onvifSettings.begin();

    for (; it != onvifSettings.end(); ++it) {
        setParam(it.key(), it.value().serializeToStr(), QnDomainPhysical);
        if (m_appStopping)
            return;
    }

    QMutexLocker lock(&m_physicalParamsMutex);

    m_onvifAdditionalSettings = std::move(settings);
}

CameraDiagnostics::Result QnPlOnvifResource::sendVideoEncoderToCamera(VideoEncoder& encoder)
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_timeDrift);

    SetVideoConfigReq request;
    SetVideoConfigResp response;
    request.Configuration = &encoder;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {

        if (soapWrapper.isNotAuthenticated())
            setStatus(QnResource::Unauthorized);

        qCritical() << "QnOnvifStreamReader::sendVideoEncoderToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        if (soapWrapper.getLastError().contains(QLatin1String("not possible to set")))
            return CameraDiagnostics::CannotConfigureMediaStreamResult( QLatin1String("fps") );   //TODO: #ak find param name
        else
            return CameraDiagnostics::CannotConfigureMediaStreamResult( QString() );
    }
    return CameraDiagnostics::NoErrorResult();
}

void QnPlOnvifResource::onRenewSubscriptionTimer()
{
    QMutexLocker lk( &m_subscriptionMutex );

    if( !m_eventCapabilities.get() )
        return;

    const QAuthenticator& auth = getAuth();
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
        sprintf( buf, "<dom0:SubscriptionId xmlns:dom0=\"(null)\">%s</dom0:SubscriptionId>", m_onvifNotificationSubscriptionID.toLatin1().data() );
        request.__any.push_back( buf );
    }
    _oasisWsnB2__RenewResponse response;
    m_prevSoapCallResult = soapWrapper.renew( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to renew subscription (endpoint %1). %2").
            arg(QString::fromLatin1(soapWrapper.endpoint())).arg(m_prevSoapCallResult), cl_logDEBUG1 );
        //TODO/IMPL creating new subscription
        lk.unlock();
        QnSoapServer::instance()->getService()->removeResourceRegistration( this );
        registerNotificationConsumer();
        return;
    }

    unsigned int renewSubsciptionTimeoutSec = response.oasisWsnB2__CurrentTime
        ? (response.oasisWsnB2__TerminationTime - *response.oasisWsnB2__CurrentTime)
        : DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;
    m_renewSubscriptionTaskID = TimerManager::instance()->addTimer(
        this,
        (renewSubsciptionTimeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS
            ? renewSubsciptionTimeoutSec-RENEW_NOTIFICATION_FORWARDING_SECS
            : renewSubsciptionTimeoutSec)*MS_PER_SECOND );

    //unsigned int renewSubsciptionTimeoutSec = utcTerminationTime - ::time(NULL);
    //renewSubsciptionTimeoutSec = renewSubsciptionTimeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS
    //    ? renewSubsciptionTimeoutSec - RENEW_NOTIFICATION_FORWARDING_SECS
    //    : 0;
    //m_timerID = TimerManager::instance()->addTimer( this, renewSubsciptionTimeoutSec*MS_PER_SECOND );
}

void QnPlOnvifResource::checkMaxFps(VideoConfigsResp& response, const QString& encoderId)
{
    VideoEncoder* vEncoder = 0;
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        if (QString::fromStdString(response.Configurations[i]->token) == encoderId)
            vEncoder = response.Configurations[i];
    }
    if (!vEncoder || !vEncoder->RateControl)
        return;

    int maxFpsOrig = getMaxFps();
    int rangeHi = getMaxFps()-2;
    int rangeLow = getMaxFps()/4;
    int currentFps = rangeHi;
    int prevFpsValue = -1;

    vEncoder->Resolution->Width = m_resolutionList[0].width();
    vEncoder->Resolution->Height = m_resolutionList[0].height();
    
    while (currentFps != prevFpsValue)
    {
        vEncoder->RateControl->FrameRateLimit = currentFps;
        bool success = false;
        bool invalidFpsDetected = false;
        for (int i = 0; i < getMaxOnvifRequestTries(); ++i)
        {
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

QnAbstractPtzController *QnPlOnvifResource::createPtzControllerInternal()
{
    if(getPtzUrl().isEmpty() || getPtzConfigurationToken().isEmpty())
        return NULL;

    QScopedPointer<QnOnvifPtzController> result(new QnOnvifPtzController(toSharedPointer(this)));
    if(result->getCapabilities() == Qn::NoPtzCapabilities)
        return NULL;
    
    return result.take();
}

bool QnPlOnvifResource::startInputPortMonitoring()
{
    if( hasFlags(QnResource::foreigner) )     //we do not own camera
    {
        return false;
    }

    if( !m_eventCapabilities.get() )
        return false;

    if( m_eventCapabilities->WSPullPointSupport )
        return createPullPointSubscription();
    else if( QnSoapServer::instance()->initialized() )
        return registerNotificationConsumer();
    else
        return false;
}

void QnPlOnvifResource::stopInputPortMonitoring()
{
    //removing timer
    if( m_timerID > 0 )
    {
        TimerManager::instance()->deleteTimer( m_timerID );
        m_timerID = 0;
    }
    if( m_renewSubscriptionTaskID > 0 )
    {
        TimerManager::instance()->deleteTimer( m_renewSubscriptionTaskID );
        m_renewSubscriptionTaskID = 0;
    }
    //TODO/IMPL removing device event registration
        //if we do not remove event registration, camera will do it for us in some timeout

    QnSoapServer::instance()->getService()->removeResourceRegistration( this );
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

QnPlOnvifResource::NotificationMessageParseHandler::NotificationMessageParseHandler()
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
            if( localName != QLatin1String("Message") )
                return false;
            int utcTimeIndex = atts.index( QLatin1String("UtcTime") );
            if( utcTimeIndex == -1 )
                return false;   //missing required attribute
            utcTime = QDateTime::fromString( atts.value(utcTimeIndex), Qt::ISODate );
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
                return false;
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

static const int DEFAULT_HTTP_PORT = 80;

bool QnPlOnvifResource::registerNotificationConsumer()
{
    QMutexLocker lk( &m_subscriptionMutex );

    //determining local address, accessible by onvif device
    QUrl eventServiceURL( QString::fromStdString(m_eventCapabilities->XAddr) );
    QString localAddress;

    //TODO: #ak should read local address only once
    std::unique_ptr<AbstractStreamSocket> sock( SocketFactory::createStreamSocket() );
    if( !sock->connect( eventServiceURL.host(), eventServiceURL.port(DEFAULT_HTTP_PORT) ) )
    {
        NX_LOG( lit("Failed to connect to %1:%2 to determine local address. %3").
            arg(eventServiceURL.host()).arg(eventServiceURL.port()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
        return false;
    }
    localAddress = sock->getLocalAddress().address.toString();

    const QAuthenticator& auth = getAuth();
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
    m_prevSoapCallResult = soapWrapper.Subscribe( &request, &response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )    //TODO/IMPL find out which is error and which is not
    {
        NX_LOG( lit("Failed to subscribe in NotificationProducer. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logWARNING );
        return false;
    }

    //TODO: #ak if this variable is unused following code may be deleted as well
    time_t utcTerminationTime; //= ::time(NULL) + DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;
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
            m_onvifNotificationSubscriptionReference = QString::fromStdString(response.SubscriptionReference->Address->__item);
    }

    //launching renew-subscription timer
    unsigned int renewSubsciptionTimeoutSec = 0;
    if( response.oasisWsnB2__CurrentTime && response.oasisWsnB2__TerminationTime )
        renewSubsciptionTimeoutSec = *response.oasisWsnB2__TerminationTime - *response.oasisWsnB2__CurrentTime;
    else
        renewSubsciptionTimeoutSec = DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT;
    m_renewSubscriptionTaskID = TimerManager::instance()->addTimer(
        this,
        (renewSubsciptionTimeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS
            ? renewSubsciptionTimeoutSec-RENEW_NOTIFICATION_FORWARDING_SECS
            : renewSubsciptionTimeoutSec)*MS_PER_SECOND );

    /* Note that we don't pass shared pointer here as this would create a 
     * cyclic reference and onvif resource will never be deleted. */
    QnSoapServer::instance()->getService()->registerResource(
        this,
        QUrl(QString::fromStdString(m_eventCapabilities->XAddr)).host(),
        m_onvifNotificationSubscriptionReference );

    m_eventMonitorType = emtNotification;

    NX_LOG( lit("Successfully registered in NotificationProducer. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
    return true;
}

bool QnPlOnvifResource::createPullPointSubscription()
{
    const QAuthenticator& auth = getAuth();
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
    m_prevSoapCallResult = soapWrapper.createPullPointSubscription( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to subscribe in NotificationProducer. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logWARNING );
        return false;
    }

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
            m_onvifNotificationSubscriptionReference = QString::fromStdString(response.SubscriptionReference->Address->__item);
    }

    //adding task to refresh subscription
    unsigned int renewSubsciptionTimeoutSec = response.oasisWsnB2__TerminationTime - response.oasisWsnB2__CurrentTime;
    m_renewSubscriptionTaskID = TimerManager::instance()->addTimer(
        this,
        (renewSubsciptionTimeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS
            ? renewSubsciptionTimeoutSec-RENEW_NOTIFICATION_FORWARDING_SECS
            : renewSubsciptionTimeoutSec)*MS_PER_SECOND );

    m_eventMonitorType = emtPullPoint;
    m_prevRequestSendClock = m_monotonicClock.elapsed();
    m_timerID = TimerManager::instance()->addTimer( this, PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND );
    return true;
}

bool QnPlOnvifResource::isInputPortMonitored() const
{
    return m_timerID != 0;
}

template<class _NumericInt>
_NumericInt roundUp( _NumericInt val, _NumericInt step, typename std::enable_if<std::is_integral<_NumericInt>::value>::type* = nullptr )
{
    if( step == 0 )
        return val;
    return (val + step - 1) / step * step;
}

bool QnPlOnvifResource::pullMessages()
{
    const QAuthenticator& auth = getAuth();
    PullPointSubscriptionWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user(),
        auth.password(),
        m_timeDrift );
    soapWrapper.getProxy()->soap->imode |= SOAP_XML_IGNORENS;

    char buf[512];

    _onvifEvents__PullMessages request;
    sprintf( buf, "PT%dS", PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC );
    //sprintf( buf, "PT1M" );
    request.Timeout = buf;
    request.MessageLimit = 1024;
    QByteArray onvifNotificationSubscriptionIDLatin1 = m_onvifNotificationSubscriptionID.toLatin1();
    strcpy( buf, onvifNotificationSubscriptionIDLatin1.data() );
    struct SOAP_ENV__Header header;
    memset( &header, 0, sizeof(header) );
    soapWrapper.getProxy()->soap->header = &header;
    soapWrapper.getProxy()->soap->header->subscriptionID = buf;
    _onvifEvents__PullMessagesResponse response;
    const qint64 currentRequestSendClock = m_monotonicClock.elapsed();
    m_prevSoapCallResult = soapWrapper.pullMessages( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to pull messages in NotificationProducer. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
        m_timerID = TimerManager::instance()->addTimer( this, PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND );
        return false;
    }

    const time_t minNotificationTime = response.CurrentTime - roundUp<qint64>(m_monotonicClock.elapsed() - m_prevRequestSendClock, 1000) / 1000;
    if( response.oasisWsnB2__NotificationMessage.size() > 0 )
    {
        for( size_t i = 0;
            i < response.oasisWsnB2__NotificationMessage.size();
            ++i )
        {
            notificationReceived( *response.oasisWsnB2__NotificationMessage[i], minNotificationTime );
        }
    }

    m_prevRequestSendClock = currentRequestSendClock;
    m_timerID = TimerManager::instance()->addTimer( this, PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND );
    return true;
}

bool QnPlOnvifResource::fetchRelayOutputs( std::vector<RelayOutputInfo>* const relayOutputs )
{
    const QAuthenticator& auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift );

    _onvifDevice__GetRelayOutputs request;
    _onvifDevice__GetRelayOutputsResponse response;
    m_prevSoapCallResult = soapWrapper.getRelayOutputs( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to get relay input/output info. endpoint %1").arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logDEBUG1 );
        return false;
    }

    m_relayOutputInfo.clear();
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

    NX_LOG( lit("Successfully got device (%1) IO ports info. Found %2 digital input and %3 relay output").
        arg(QString::fromLatin1(soapWrapper.endpoint())).arg(0).arg(m_relayOutputInfo.size()), cl_logDEBUG1 );

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

    return false;
    return false; //there is no output with id outputID
}

bool QnPlOnvifResource::setRelayOutputSettings( const RelayOutputInfo& relayOutputInfo )
{
    const QAuthenticator& auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift );

    NX_LOG( lit("Swiching camera %1 relay output %2 to monostable mode").
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
    m_prevSoapCallResult = soapWrapper.setRelayOutputSettings( setOutputSettingsRequest, setOutputSettingsResponse );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to switch camera %1 relay output %2 to monostable mode. %3").
            arg(QString::fromLatin1(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)).arg(m_prevSoapCallResult), cl_logWARNING );
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

bool QnPlOnvifResource::secondaryResolutionIsLarge() const
{ 
    return m_secondaryResolution.width() * m_secondaryResolution.height() > 720 * 480;
}

bool QnPlOnvifResource::setRelayOutputStateNonSafe(
    const QString& outputID,
    bool active,
    unsigned int autoResetTimeoutMS )
{
    //retrieving output info to check mode
    RelayOutputInfo relayOutputInfo;
    if( !fetchRelayOutputInfo( outputID.toStdString(), &relayOutputInfo ) )
    {
        NX_LOG( lit("Cannot change relay output %1 state. Failed to get relay output info").arg(outputID), cl_logWARNING );
        return false;
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
            NX_LOG( lit("Cannot set camera %1 output %2 to state %3 with timeout %4 msec. Cannot set mode to %5. %6").
                arg(QString()).arg(QString::fromStdString(relayOutputInfo.token)).arg(QLatin1String(active ? "active" : "inactive")).arg(autoResetTimeoutMS).
                arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")).arg(m_prevSoapCallResult), cl_logWARNING );
            return false;
        }

        NX_LOG( lit("Camera %1 output %2 has been switched to %3 mode").arg(QString()).arg(outputID).
            arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")), cl_logWARNING );
    }

    //modifying output
    const QAuthenticator& auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user(),
        auth.password(),
        m_timeDrift );

    _onvifDevice__SetRelayOutputState request;
    request.RelayOutputToken = relayOutputInfo.token;
    request.LogicalState = active ? onvifXsd__RelayLogicalState__active : onvifXsd__RelayLogicalState__inactive;
    _onvifDevice__SetRelayOutputStateResponse response;
    m_prevSoapCallResult = soapWrapper.setRelayOutputState( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        NX_LOG( lit("Failed to set relay %1 output state to %2. endpoint %3").
            arg(QString::fromStdString(relayOutputInfo.token)).arg(active).arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logWARNING );
        return false;
    }

#ifdef SIMULATE_RELAY_PORT_MOMOSTABLE_MODE
    if( autoResetTimeoutMS > 0 )
    {
        //adding task to reset port state
        const quint64 timerID = TimerManager::instance()->addTimer( this, autoResetTimeoutMS );
        if (active)
            m_triggerOutputTasks.insert( std::make_pair( timerID, TriggerOutputTask( outputID, !active, 0 ) ) );
    }
#endif

    NX_LOG( lit("Successfully set relay %1 output state to %2. endpoint %3").
        arg(QString::fromStdString(relayOutputInfo.token)).arg(active).arg(QString::fromLatin1(soapWrapper.endpoint())), cl_logWARNING );
    return true;
}

QMutex* QnPlOnvifResource::getStreamConfMutex()
{
    return &m_streamConfMutex;
}

void QnPlOnvifResource::beforeConfigureStream()
{
    QMutexLocker lock (&m_streamConfMutex);
    ++m_streamConfCounter;
    while (m_streamConfCounter > 1)
        m_streamConfCond.wait(&m_streamConfMutex);
}

void QnPlOnvifResource::afterConfigureStream()
{
    QMutexLocker lock (&m_streamConfMutex);
    --m_streamConfCounter;
    m_streamConfCond.wakeAll();
    while (m_streamConfCounter > 0)
        m_streamConfCond.wait(&m_streamConfMutex);

}

CameraDiagnostics::Result QnPlOnvifResource::getFullUrlInfo()
{
    QAuthenticator auth(getAuth());
    //TODO: #vasilenko UTF unuse StdString
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), m_timeDrift);

    //Trying to get onvif URLs
    CapabilitiesReq request;
    CapabilitiesResp response;

    int soapRes = soapWrapper.getCapabilities(request, response);
    if (soapRes != SOAP_OK) 
    {
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
            << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        if (soapWrapper.isNotAuthenticated())
        {
            setStatus(QnResource::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult( getDeviceOnvifUrl() );
        }
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getCapabilities"), soapWrapper.getLastError());
    }

    if (response.Capabilities) 
    {
        //TODO: #vasilenko UTF unuse std::string
        if (response.Capabilities->Events)
            m_eventCapabilities.reset( new onvifXsd__EventCapabilities( *response.Capabilities->Events ) );

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
        m_deviceIOUrl = response.Capabilities->Extension && response.Capabilities->Extension->DeviceIO
            ? response.Capabilities->Extension->DeviceIO->XAddr
            : getDeviceOnvifUrl().toStdString();
    }

    return CameraDiagnostics::NoErrorResult();
}

#endif //ENABLE_ONVIF
