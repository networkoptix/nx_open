
#include <climits>
#include <cmath>
#include <sstream>

#include <QDebug>
#include <QHash>
#include <QTimer>

#include <onvif/soapDeviceBindingProxy.h>
#include <onvif/soapMediaBindingProxy.h>
#include <onvif/soapNotificationProducerBindingProxy.h>
#include <onvif/soapEventBindingProxy.h>
#include <onvif/soapPullPointSubscriptionBindingProxy.h>

#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "onvif_helper.h"
#include "utils/common/synctime.h"
#include "utils/math/math.h"
#include "utils/common/timermanager.h"
#include "api/app_server_connection.h"
#include <business/business_event_connector.h>
#include "soap/soapserver.h"
#include "soapStub.h"


const char* QnPlOnvifResource::MANUFACTURE = "OnvifDevice";
static const float MAX_EPS = 0.01f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifResource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifResource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifResource::DEFAULT_IFRAME_DISTANCE = 20;
QString QnPlOnvifResource::MEDIA_URL_PARAM_NAME = QLatin1String("MediaUrl");
QString QnPlOnvifResource::ONVIF_URL_PARAM_NAME = QLatin1String("DeviceUrl");
QString QnPlOnvifResource::MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
QString QnPlOnvifResource::AUDIO_SUPPORTED_PARAM_NAME = QLatin1String("isAudioSupported");
QString QnPlOnvifResource::DUAL_STREAMING_PARAM_NAME = QLatin1String("hasDualStreaming");
const float QnPlOnvifResource::QUALITY_COEF = 0.2f;
const int QnPlOnvifResource::MAX_AUDIO_BITRATE = 64; //kbps
const int QnPlOnvifResource::MAX_AUDIO_SAMPLERATE = 32; //khz
const int QnPlOnvifResource::ADVANCED_SETTINGS_VALID_TIME = 200; //200 ms
static const unsigned int DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT = 15;
//!if renew subscription exactly at termination time, camera can already terminate subscription, so have to do that a little bit earlier..
static const unsigned int RENEW_NOTIFICATION_FORWARDING_SECS = 5;
static const unsigned int MS_PER_SECOND = 1000;
static const unsigned int PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC = 1;

//Forth times greater than default = 320 x 240

static const int MAX_PRIMARY_RES_FOR_SOFT_MOTION = 720 * 576;


/* Some cameras declare invalid max resolution */
struct StrictResolution {
    const char* model;
    QSize maxRes;
};

StrictResolution strictResolutionList[] =
{
    { "Brickcom-30xN", QSize(1920, 1080) }
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
    int square1Min = INT_MAX;
    for (int i = 0; i < s1.resolutions.size(); ++i) {
        square1Max = qMax(square1Max, s1.resolutions[i].width() * s1.resolutions[i].height());
        square1Min = qMin(square1Min, s1.resolutions[i].width() * s1.resolutions[i].height());
    }
    
    int square2Max = 0;
    int square2Min = INT_MAX;
    for (int i = 0; i < s2.resolutions.size(); ++i) {
        square2Max = qMax(square2Max, s2.resolutions[i].width() * s2.resolutions[i].height());
        square2Min = qMin(square2Min, s2.resolutions[i].width() * s2.resolutions[i].height());
    }

    if (square1Max != square2Max)
        return square1Max > square2Max;

    if (square1Min != square2Min)
        return square1Min > square2Min;

    // if some option doesn't have H264 it "less"
    if (!s1.isH264 && s2.isH264)
        return true;
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
    m_onvifAdditionalSettings(0),
    m_physicalParamsMutex(QMutex::Recursive),
    m_advSettingsLastUpdated(QDateTime::currentDateTime()),
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
    m_eventCapabilities( NULL ),
    m_eventMonitorType( emtNone ),
    m_timerID( 0 ),
    m_renewSubscriptionTaskID(0),
    m_maxChannels(1)
{
    connect(
        this, SIGNAL(cameraInput(QnResourcePtr, const QString&, bool, qint64)), 
        QnBusinessEventConnector::instance(), SLOT(at_cameraInput( QnResourcePtr, const QString&, bool, qint64 )) );
}

QnPlOnvifResource::~QnPlOnvifResource()
{
    stopInputPortMonitoring();

    delete m_onvifAdditionalSettings;

    if( m_eventCapabilities )
    {
        delete m_eventCapabilities;
        m_eventCapabilities = NULL;
    }

    QMutexLocker lk( &m_subscriptionMutex );
    while( !m_triggerOutputTasks.empty() )
    {
        const quint64 timerID = m_triggerOutputTasks.begin()->first;
        m_triggerOutputTasks.erase( m_triggerOutputTasks.begin() );
        lk.unlock();
        TimerManager::instance()->joinAndDeleteTimer( timerID );    //garantees that no onTimer(timerID) is running on return
        lk.relock();
    }
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
                //TODO:UTF unuse std::string
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
                //TODO:UTF unuse std::string
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

QString QnPlOnvifResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

bool QnPlOnvifResource::hasDualStreaming() const
{
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlOnvifResource*>(this);
    this_casted->getParam(DUAL_STREAMING_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toInt();
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

void QnPlOnvifResource::setCropingPhysical(QRect /*croping*/)
{

}

bool QnPlOnvifResource::initInternal()
{
    setCodec(H264, true);
    setCodec(H264, false);

    if (getDeviceOnvifUrl().isEmpty()) {
        qCritical() << "QnPlOnvifResource::initInternal: Can't do anything: ONVIF device url is absent. Id: " << getPhysicalId();
        return false;
    }

    calcTimeDrift();

    if (m_appStopping)
        return false;

    if (getImagingUrl().isEmpty() || getMediaUrl().isEmpty() || getName().contains(QLatin1String("Unknown")) || getMAC().isEmpty() || m_needUpdateOnvifUrl)
    {
        if (!fetchAndSetDeviceInformation(false) && getMediaUrl().isEmpty())
        {
            qCritical() << "QnPlOnvifResource::initInternal: ONVIF media url is absent. Id: " << getPhysicalId();
            return false;
        }
        else
            m_needUpdateOnvifUrl = false;
    }

    if (m_appStopping)
        return false;

    if (!fetchAndSetVideoSource())
        return false;

    if (m_appStopping)
        return false;

    fetchAndSetAudioSource();

    if (m_appStopping)
        return false;

    if (!fetchAndSetResourceOptions()) 
    {
        m_needUpdateOnvifUrl = true;
        return false;
    }

    if (m_appStopping)
        return false;

    //if (getStatus() == QnResource::Offline || getStatus() == QnResource::Unauthorized)
    //    setStatus(QnResource::Online, true); // to avoid infinit status loop in this version

    //Additional camera settings
    fetchAndSetCameraSettings();

    if (m_appStopping)
        return false;

    Qn::CameraCapabilities addFlags = Qn::NoCapabilities;
    if (m_ptzController)
        addFlags |= m_ptzController->getCapabilities();
    if (m_primaryResolution.width() * m_primaryResolution.height() <= MAX_PRIMARY_RES_FOR_SOFT_MOTION)
        addFlags |= Qn::PrimaryStreamSoftMotionCapability;
    else if (!hasDualStreaming())
        setMotionType(Qn::MT_NoMotion);

    
    if (addFlags != Qn::NoCapabilities)
        setCameraCapabilities(getCameraCapabilities() | addFlags);

    //registering onvif event handler
    std::vector<QnPlOnvifResource::RelayOutputInfo> relayOutputs;
    fetchRelayOutputs( &relayOutputs );
    if( !relayOutputs.empty() )
    {
        setCameraCapability( Qn::RelayOutputCapability, true );
        setCameraCapability( Qn::RelayInputCapability, true );    //TODO it's not clear yet how to get input port list for sure (on DW cam getDigitalInputs returns nothing)
    }

    if (m_appStopping)
        return false;

    fetchRelayInputInfo();

    if (m_appStopping)
        return false;

    //if( !m_relayInputs.empty() )
    //    setCameraCapability( Qn::relayInput, true );

    save();

    return true;
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

void QnPlOnvifResource::checkPrimaryResolution(QSize& primaryResolution)
{
    for (uint i = 0; i < sizeof(strictResolutionList) / sizeof(StrictResolution); ++i)
    {
        if (getModel() == QLatin1String(strictResolutionList[i].model))
            primaryResolution = strictResolutionList[i].maxRes;
    }
}

void QnPlOnvifResource::fetchAndSetPrimarySecondaryResolution()
{
    QMutexLocker lock(&m_mutex);

    if (m_resolutionList.isEmpty()) {
        return;
    }

    m_primaryResolution = m_resolutionList.front();
    checkPrimaryResolution(m_primaryResolution);
    float currentAspect = getResolutionAspectRatio(m_primaryResolution);

    m_secondaryResolution = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect);
    if (m_secondaryResolution == EMPTY_RESOLUTION_PAIR)
        m_secondaryResolution = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0); // try to get resolution ignoring aspect ration

    qDebug() << "ONVIF debug: got secondary resolution" << m_secondaryResolution << "encoders for camera " << getHostAddress();


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

int QnPlOnvifResource::getMaxFps()
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

bool QnPlOnvifResource::fetchAndSetDeviceInformation(bool performSimpleCheck)
{
    QAuthenticator auth(getAuth());
    //TODO:UTF unuse StdString
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    QString user = auth.user();
    QString password = auth.password();
    QString hardwareId;
    
    //Trying to get name
    //if (getName().isEmpty() || getModel().isEmpty() || getFirmware().isEmpty())
    if (1)
    {
        DeviceInfoReq request;
        DeviceInfoResp response;

        int soapRes = soapWrapper.getDeviceInformation(request, response);
        if (soapRes != SOAP_OK) 
        {
            qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
                << soapWrapper.getEndpointUrl() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastError();

            return false;
        } 
        else
        {
            if (getName().isEmpty())
                setName(QString::fromStdString(response.Manufacturer) + QLatin1String(" - ") + QString::fromStdString(response.Model));
            if (getModel().isEmpty())
                setModel(QLatin1String(response.Model.c_str()));
            setFirmware(QLatin1String(response.FirmwareVersion.c_str()));
            hardwareId = QString::fromStdString(response.HardwareId);

            if (performSimpleCheck)
                return true;
        }
    }

    //Trying to get onvif URLs
    {
        CapabilitiesReq request;
        CapabilitiesResp response;

        int soapRes = soapWrapper.getCapabilities(request, response);
        if (soapRes != SOAP_OK) 
        {
            qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
                << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
            if (soapWrapper.isNotAuthenticated())
                setStatus(QnResource::Unauthorized);
            return false;
        }

        if (response.Capabilities) 
        {
            //TODO:UTF unuse std::string
            if (response.Capabilities->Events)
                m_eventCapabilities = new onvifXsd__EventCapabilities( *response.Capabilities->Events );

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
                setPtzfUrl(fromOnvifDiscoveredUrl(response.Capabilities->PTZ->XAddr));
            }
            m_deviceIOUrl = response.Capabilities->Extension && response.Capabilities->Extension->DeviceIO
                ? response.Capabilities->Extension->DeviceIO->XAddr
                : getDeviceOnvifUrl().toStdString();
        }
    }

    //Trying to get MAC
    {
        NetIfacesReq request;
        NetIfacesResp response;

        int soapRes = soapWrapper.getNetworkInterfaces(request, response);
        if( soapRes != SOAP_OK )
        {
            qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch MAC address. Reason: SOAP to endpoint "
                << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
            return false;
        }

        const QString& mac = fetchMacAddress(response, QUrl(getDeviceOnvifUrl()).host());

        if (!mac.isEmpty()) 
            setMAC(mac);

        if (getPhysicalId().isEmpty())
        {
            setPhysicalId(hardwareId);
        }
    }

    return true;
}

/*
    if defined, expecting following notification:

    <tt:Message UtcTime="2012-12-13T04:00:57Z" PropertyOperation="Changed">
        <tt:Source>
            <tt:SimpleItem Value="0" Name="RelayToken">
            </tt:SimpleItem>
        </tt:Source>
        <tt:Data>
            <tt:SimpleItem Value="active" Name="LogicalState">
            </tt:SimpleItem>
        </tt:Data>
    </tt:Message>
*/
//#define MONITOR_TRIGGER_RELAY_NOTIFICATION

void QnPlOnvifResource::notificationReceived( const oasisWsnB2__NotificationMessageHolderType& notification )
{
    if( !notification.Message.__any )
    {
        cl_log.log( QString::fromLatin1("Received notification with empty message. Ignoring..."), cl_logDEBUG2 );
        return;
    }

    if( !notification.oasisWsnB2__Topic ||
        !notification.oasisWsnB2__Topic->__item )
    {
        cl_log.log( QString::fromLatin1("Received notification with no topic specified. Ignoring..."), cl_logDEBUG2 );
        return;
    }

#ifdef MONITOR_TRIGGER_RELAY_NOTIFICATION
    if( strcmp(notification.oasisWsnB2__Topic->__item, "tns:Device/Trigger/Relay") != 0 )
#else
    if( strcmp(notification.oasisWsnB2__Topic->__item, "tns:Device/tnsw4n:IO/Port") != 0 )
#endif
    {
        cl_log.log( QString::fromLatin1("Received notification with unknown topic: %1. Ignoring...").
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

    //checking that there is single source is a port
    std::list<NotificationMessageParseHandler::SimpleItem>::const_iterator portSourceIter = handler.source.end();
    for( std::list<NotificationMessageParseHandler::SimpleItem>::const_iterator
        it = handler.source.begin();
        it != handler.source.end();
        ++it )
    {
#ifdef MONITOR_TRIGGER_RELAY_NOTIFICATION
        if( it->name == QLatin1String("RelayToken") )
#else
        if( it->name == QLatin1String("port") )
#endif
        {
            portSourceIter = it;
            break;
        }
    }

    if( portSourceIter == handler.source.end()  //source is not port
#ifdef MONITOR_TRIGGER_RELAY_NOTIFICATION
        || handler.data.name != QLatin1String("LogicalState") )
#else
        || handler.data.name != QLatin1String("state") )
#endif
    {
        return;
    }

    //saving port state
#ifdef MONITOR_TRIGGER_RELAY_NOTIFICATION
    const bool newPortState = handler.data.value == QLatin1String("active");
#else
    const bool newPortState = handler.data.value == QLatin1String("true");
#endif
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

bool QnPlOnvifResource::fetchAndSetResourceOptions()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    if (!fetchAndSetVideoEncoderOptions(soapWrapper)) {
        return false;
    }

    if (!updateResourceCapabilities()) {
        return false;
    }

    //All VideoEncoder options are set, so we can calculate resolutions for the streams
    fetchAndSetPrimarySecondaryResolution();

    //Before invoking <fetchAndSetHasDualStreaming> Primary and Secondary Resolutions MUST be set
    fetchAndSetDualStreaming(soapWrapper);

    if (fetchAndSetAudioEncoder(soapWrapper) && fetchAndSetAudioEncoderOptions(soapWrapper))
        setParam(AUDIO_SUPPORTED_PARAM_NAME, 1, QnDomainDatabase);
    else
        setParam(AUDIO_SUPPORTED_PARAM_NAME, 0, QnDomainDatabase);

    return true;
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

        qDebug() << "ONVIF quality range [" << m_minQuality << ", " << m_maxQuality << "]";
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
        qDebug() << "ONVIF iframe distance: " << m_iframeDistance;
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
        qDebug() << "ONVIF resolutions: ";
        foreach (QSize resolution, m_resolutionList) {
            qDebug() << resolution.width() << " x " << resolution.height();
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
        qDebug() << "ONVIF resolutions: ";
        foreach (QSize resolution, m_resolutionList) {
            qDebug() << resolution.width() << " x " << resolution.height();
        }
    }
}

int QnPlOnvifResource::innerQualityToOnvif(QnStreamQuality quality) const
{
    if (quality > QnQualityHighest) {
        qWarning() << "QnPlOnvifResource::innerQualityToOnvif: got unexpected quality (too big): " << quality;
        return m_maxQuality;
    }
    if (quality < QnQualityLowest) {
        qWarning() << "QnPlOnvifResource::innerQualityToOnvif: got unexpected quality (too small): " << quality;
        return m_minQuality;
    }

    qDebug() << "QnPlOnvifResource::innerQualityToOnvif: in quality = " << quality << ", out qualty = "
             << m_minQuality + (m_maxQuality - m_minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest)
             << ", minOnvifQuality = " << m_minQuality << ", maxOnvifQuality = " << m_maxQuality;

    return m_minQuality + (m_maxQuality - m_minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
}

/*
bool QnPlOnvifResource::isSoapAuthorized() const 
{
    QAuthenticator auth(getAuth());
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

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
    DeviceSoapWrapper soapWrapper(deviceUrl.toStdString(), "", "", 0);

    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = soapWrapper.GetSystemDateAndTime(request, response);

    if (soapRes == SOAP_OK && response.SystemDateAndTime->UTCDateTime)
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

void QnPlOnvifResource::setPtzfUrl(const QString& src) 
{
    QMutexLocker lock(&m_mutex);
    m_ptzUrl = src;
}

QString QnPlOnvifResource::getPtzfUrl() const
{
    QMutexLocker lock(&m_mutex);
    return m_ptzUrl;
}

void QnPlOnvifResource::setMinMaxQuality(int min, int max)
{
    int netoptixDelta = QnQualityHighest - QnQualityLowest;
    int onvifDelta = max - min;

    if (netoptixDelta < 0 || onvifDelta < 0) {
        qWarning() << "QnPlOnvifResource::setMinMaxQuality: incorrect values: min > max: onvif ["
                   << min << ", " << max << "] netoptix [" << QnQualityLowest << ", " << QnQualityHighest << "]";
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


bool QnPlOnvifResource::shoudResolveConflicts() const
{
    return false;
}

bool QnPlOnvifResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source)
{
    QnPlOnvifResourcePtr onvifR = source.dynamicCast<QnPlOnvifResource>();
    if (!onvifR)
        return false;

    QString onvifUrlSource = onvifR->getDeviceOnvifUrl();
    QString mediaUrlSource = onvifR->getMediaUrl();

    bool result = QnPhysicalCameraResource::mergeResourcesIfNeeded(source);

    if (getGroupId() != onvifR->getGroupId())
    {
        setGroupId(onvifR->getGroupId());
        result = true; // groupID can be changed for onvif resource because if not auth info, maxChannels is not accessible
    }

    if (getGroupName().isEmpty() && getGroupName() != onvifR->getGroupName())
    {
        setGroupName(onvifR->getGroupName());
        result = true;
    }

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
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

    _onvifDeviceIO__GetDigitalInputs request;
    _onvifDeviceIO__GetDigitalInputsResponse response;
    m_prevSoapCallResult = soapWrapper.getDigitalInputs( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        cl_log.log( QString::fromAscii("Failed to get relay digital input list. endpoint %1").arg(QString::fromAscii(soapWrapper.endpoint())), cl_logWARNING );
        return true;
    }

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

bool QnPlOnvifResource::fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper)
{
    VideoConfigsReq confRequest;
    VideoConfigsResp confResponse;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse); // get encoder list
    if (soapRes != SOAP_OK) {
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't get list of video encoders from camera (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << "). GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    std::string login = soapWrapper.getLogin();
    std::string password = soapWrapper.getPassword();
    std::string endpoint = soapWrapper.getEndpointUrl().toStdString();

    int confRangeStart = 0;
    int confRangeEnd = (int) confResponse.Configurations.size();
    if (m_maxChannels > 1)
    {
        // determine amount encoder configurations per each video source
        confRangeStart = confRangeEnd/m_maxChannels * getChannel();
        confRangeEnd = confRangeStart + confRangeEnd/m_maxChannels;
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
            return false;
    }

    qSort(optionsList.begin(), optionsList.end(), videoOptsGreaterThan);

    if (optionsList.isEmpty())
    {
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: all video options are empty. (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << ").";

        return false;
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
    if (m_maxChannels == 1)
        checkMaxFps(confResponse, optionsList[0].id);

    m_mutex.lock();
    m_primaryVideoEncoderId = optionsList[0].id;
    m_secondaryResolutionList = m_resolutionList;
    m_mutex.unlock();

    qDebug() << "ONVIF debug: got" << optionsList.size() << "encoders for camera " << getHostAddress();

    bool dualStreamingAllowed = optionsList.size() >= 2;
    if (dualStreamingAllowed)
    {
        int secondaryIndex = 1;
        QMutexLocker lock(&m_mutex);

        m_secondaryVideoEncoderId = optionsList[secondaryIndex].id;
        if (optionsList[secondaryIndex].isH264) {
            m_secondaryH264Profile = getH264StreamProfile(optionsList[secondaryIndex]);
                setCodec(H264, false);
                qDebug() << "use H264 codec for secondary stream. camera=" << getHostAddress();
            }
            else {
                setCodec(JPEG, false);
                qDebug() << "use JPEG codec for secondary stream. camera=" << getHostAddress();
            }
        updateSecondaryResolutionList(optionsList[secondaryIndex]);
    }

    return true;
}

bool QnPlOnvifResource::fetchAndSetDualStreaming(MediaSoapWrapper& /*soapWrapper*/)
{
    QMutexLocker lock(&m_mutex);

    bool dualStreaming = m_secondaryResolution != EMPTY_RESOLUTION_PAIR && !m_secondaryVideoEncoderId.isEmpty();
    if (dualStreaming)
        qDebug() << "ONVIF debug: enable dualstreaming for camera" << getHostAddress();
    else {
        QString reason = m_secondaryResolution == EMPTY_RESOLUTION_PAIR ? QLatin1String("no secondary resolution") : QLatin1String("no secondary encoder");
        qDebug() << "ONVIF debug: disable dualstreaming for camera" << getHostAddress() << "reason:" << reason;
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

bool QnPlOnvifResource::updateResourceCapabilities()
{
    QMutexLocker lock(&m_mutex);

    if (!m_videoSourceSize.isValid()) {
        return true;
    }

    QList<QSize>::iterator it = m_resolutionList.begin();
    while (it != m_resolutionList.end())
    {
        if (it->width() > m_videoSourceSize.width() || it->height() > m_videoSourceSize.height())
            it = m_resolutionList.erase(it);
        else
            return true;
        }

    return true;
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
            //TODO:UTF unuse std::string
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

bool QnPlOnvifResource::sendVideoSourceToCamera(VideoSource* source) const
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), getTimeDrift());

    SetVideoSrcConfigReq request;
    SetVideoSrcConfigResp response;
    request.Configuration = source;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::setVideoSourceConfiguration: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

bool QnPlOnvifResource::detectVideoSourceCount()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

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
    return true;
}

bool QnPlOnvifResource::fetchVideoSourceToken()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    _onvifMedia__GetVideoSources request;
    _onvifMedia__GetVideoSourcesResponse response;
    int soapRes = soapWrapper.getVideoSources(request, response);

    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        if (soapWrapper.isNotAuthenticated())
            setStatus(QnResource::Unauthorized);
        return false;

    }

    m_maxChannels = (int) response.VideoSources.size();

    if (m_maxChannels <= getChannel()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;
    } 

    onvifXsd__VideoSource* conf = response.VideoSources.at(getChannel());

        if (conf) {
            QMutexLocker lock(&m_mutex);
        m_videoSourceToken = QString::fromStdString(conf->token);
        //m_videoSourceSize = QSize(conf->Resolution->Width, conf->Resolution->Height);
        return true;
    }
    return false;
}

QRect QnPlOnvifResource::getVideoSourceMaxSize(const QString& configToken)
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

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

bool QnPlOnvifResource::fetchAndSetVideoSource()
{
    if (!fetchVideoSourceToken())
        return false;

    if (m_appStopping)
        return false;

    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    VideoSrcConfigsReq request;
    VideoSrcConfigsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;

    }

    if (m_appStopping)
        return false;

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
        if (maxRect.isValid() && currentRect != maxRect)
        {
            updateVideoSource(conf, maxRect);
            return sendVideoSourceToCamera(conf);
        }
        else {
            return true;
        }

        if (m_appStopping)
            return false;
    }

    return false;
}

bool QnPlOnvifResource::fetchAndSetAudioSource()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    AudioSrcConfigsReq request;
    AudioSrcConfigsResp response;

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {

        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;

    }

    if ((int)response.Configurations.size() <= getChannel()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;
    } else {
        onvifXsd__AudioSourceConfiguration* conf = response.Configurations.at(getChannel());
        if (conf) {
            QMutexLocker lock(&m_mutex);
            //TODO:UTF unuse std::string
            m_audioSourceId = QString::fromStdString(conf->token);
            return true;
        }
    }

    return false;
}

const QnResourceAudioLayout* QnPlOnvifResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
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
    QMutexLocker lock(&m_physicalParamsMutex);
    if (!m_onvifAdditionalSettings) {
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
    QDateTime currTime = QDateTime::currentDateTime().addMSecs(-ADVANCED_SETTINGS_VALID_TIME);
    if (currTime > m_advSettingsLastUpdated) {
        if (!m_onvifAdditionalSettings->makeGetRequest()) {
            return false;
        }
        m_advSettingsLastUpdated = QDateTime::currentDateTime();
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
    if (!m_onvifAdditionalSettings) {
        return false;
    }

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
        qDebug() << "QnPlOnvifResource::fetchAndSetCameraSettings: imaging service is absent on device (URL: "
            << getDeviceOnvifUrl() << ", UniqId: " << getUniqueId() << ").";
    }

    QAuthenticator auth(getAuth());
    OnvifCameraSettingsResp* settings = new OnvifCameraSettingsResp(getDeviceOnvifUrl().toLatin1().data(), imagingUrl.toLatin1().data(),
        auth.user().toStdString(), auth.password().toStdString(), m_videoSourceToken.toStdString(), getUniqueId(), m_timeDrift);

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


    if (!m_ptzController) 
    {
        QScopedPointer<QnOnvifPtzController> controller(new QnOnvifPtzController(this));
        if (!controller->getPtzConfigurationToken().isEmpty())
            m_ptzController.reset(controller.take());
    }

    QMutexLocker lock(&m_physicalParamsMutex);

    if (m_onvifAdditionalSettings) {
        delete m_onvifAdditionalSettings;
    }

    m_onvifAdditionalSettings = settings;    
}

int QnPlOnvifResource::sendVideoEncoderToCamera(VideoEncoder& encoder) const
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    SetVideoConfigReq request;
    SetVideoConfigResp response;
    request.Configuration = &encoder;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::sendVideoEncoderToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        if (soapWrapper.getLastError().contains(QLatin1String("not possible to set")))
            soapRes = -2;
    }
    return soapRes;
}

void QnPlOnvifResource::onRenewSubscriptionTimer()
{
    QMutexLocker lk( &m_subscriptionMutex );

    if( !m_eventCapabilities || m_onvifNotificationSubscriptionID.isEmpty() )
        return;

    const QAuthenticator& auth = getAuth();
    SubscriptionManagerSoapWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

    char buf[256];

    _oasisWsnB2__Renew request;
    sprintf( buf, "PT%dS", DEFAULT_NOTIFICATION_CONSUMER_REGISTRATION_TIMEOUT );
    std::string initialTerminationTime = buf;
    request.TerminationTime = &initialTerminationTime;
    sprintf( buf, "<dom0:SubscriptionId xmlns:dom0=\"(null)\">%s</dom0:SubscriptionId>", m_onvifNotificationSubscriptionID.toLocal8Bit().data() );
    request.__any.push_back( buf );
    _oasisWsnB2__RenewResponse response;
    m_prevSoapCallResult = soapWrapper.renew( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        cl_log.log( QString::fromAscii("Failed to renew subscription (endpoint %1). %2").
            arg(QString::fromAscii(soapWrapper.endpoint())).arg(m_prevSoapCallResult), cl_logWARNING );
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
            int errCode = sendVideoEncoderToCamera(*vEncoder);
            if (errCode == SOAP_OK) 
            {
                if (currentFps >= maxFpsOrig-2) {
                    // If first try success, does not change maxFps at all. (HikVision has working range 0..15, and 25 fps, so try from max-1 checking)
                    return; 
                }
                setMaxFps(currentFps);
                success = true;
                break;
            }
            else if (errCode == -2)
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

QnAbstractPtzController* QnPlOnvifResource::getPtzController()
{
    return m_ptzController.data();
}

bool QnPlOnvifResource::startInputPortMonitoring()
{
    if( isDisabled()
        || hasFlags(QnResource::foreigner) )     //we do not own camera
    {
        return false;
    }

    if( !m_eventCapabilities )
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
    TCPSocket sock( eventServiceURL.host(), eventServiceURL.port(DEFAULT_HTTP_PORT) );
    if( !sock.isConnected() )
    {
        cl_log.log( QString::fromLatin1("Failed to connect to %1:%2 to determine local address. %3").
            arg(eventServiceURL.host()).arg(eventServiceURL.port()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
        return false;
    }
    localAddress = sock.getLocalAddress();

    const QAuthenticator& auth = getAuth();
    NotificationProducerSoapWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

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
        cl_log.log( QString::fromAscii("Failed to subscribe in NotificationProducer. endpoint %1").arg(QString::fromAscii(soapWrapper.endpoint())), cl_logWARNING );
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
    if( response.SubscriptionReference &&
        response.SubscriptionReference->ns1__ReferenceParameters &&
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
        QUrl(QString::fromStdString(m_eventCapabilities->XAddr)).host() );

    m_eventMonitorType = emtNotification;

    cl_log.log( QString::fromAscii("Successfully registered in NotificationProducer. endpoint %1").arg(QString::fromAscii(soapWrapper.endpoint())), cl_logDEBUG1 );
    return true;
}

bool QnPlOnvifResource::createPullPointSubscription()
{
    const QAuthenticator& auth = getAuth();
    EventSoapWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

    _onvifEvents__CreatePullPointSubscription request;
    std::string initialTerminationTime = "PT600S";
    request.InitialTerminationTime = &initialTerminationTime;
    _onvifEvents__CreatePullPointSubscriptionResponse response;
    m_prevSoapCallResult = soapWrapper.createPullPointSubscription( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        cl_log.log( QString::fromAscii("Failed to subscribe in NotificationProducer. endpoint %1").arg(QString::fromAscii(soapWrapper.endpoint())), cl_logWARNING );
        return false;
    }

    std::string subscriptionID;
    if( response.SubscriptionReference &&
        response.SubscriptionReference->ns1__ReferenceParameters &&
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

    //adding task to refresh subscription
    unsigned int renewSubsciptionTimeoutSec = response.oasisWsnB2__TerminationTime - response.oasisWsnB2__CurrentTime;
    m_renewSubscriptionTaskID = TimerManager::instance()->addTimer(
        this,
        (renewSubsciptionTimeoutSec > RENEW_NOTIFICATION_FORWARDING_SECS
            ? renewSubsciptionTimeoutSec-RENEW_NOTIFICATION_FORWARDING_SECS
            : renewSubsciptionTimeoutSec)*MS_PER_SECOND );

    m_eventMonitorType = emtPullPoint;

    m_timerID = TimerManager::instance()->addTimer( this, PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND );
    return true;
}

bool QnPlOnvifResource::isInputPortMonitored() const
{
    return m_timerID != 0;
}

bool QnPlOnvifResource::pullMessages()
{
    const QAuthenticator& auth = getAuth();
    PullPointSubscriptionWrapper soapWrapper(
        m_eventCapabilities->XAddr,
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

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
    m_prevSoapCallResult = soapWrapper.pullMessages( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        cl_log.log( QString::fromAscii("Failed to pull messages in NotificationProducer. endpoint %1").arg(QString::fromAscii(soapWrapper.endpoint())), cl_logDEBUG1 );
        m_timerID = TimerManager::instance()->addTimer( this, PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND );
        return false;
    }

    if( response.oasisWsnB2__NotificationMessage.size() > 0 )
    {
        for( size_t i = 0;
            i < response.oasisWsnB2__NotificationMessage.size();
            ++i )
        {
            notificationReceived( *response.oasisWsnB2__NotificationMessage[i] );
        }
    }

    m_timerID = TimerManager::instance()->addTimer( this, PULLPOINT_NOTIFICATION_CHECK_TIMEOUT_SEC*MS_PER_SECOND );
    return true;
}

bool QnPlOnvifResource::fetchRelayOutputs( std::vector<RelayOutputInfo>* const relayOutputs )
{
    const QAuthenticator& auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

    _onvifDevice__GetRelayOutputs request;
    _onvifDevice__GetRelayOutputsResponse response;
    m_prevSoapCallResult = soapWrapper.getRelayOutputs( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        cl_log.log( QString::fromAscii("Failed to get relay input/output info. endpoint %1").arg(QString::fromAscii(soapWrapper.endpoint())), cl_logWARNING );
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

    cl_log.log( QString::fromAscii("Successfully got device (%1) IO ports info. Found %2 digital input and %3 relay output").
        arg(QString::fromAscii(soapWrapper.endpoint())).arg(0).arg(m_relayOutputInfo.size()), cl_logDEBUG1 );

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
    const QAuthenticator& auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

    cl_log.log( QString::fromLatin1("Swiching camera %1 relay output %2 to monostable mode").
        arg(QString::fromAscii(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)), cl_logDEBUG1 );

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
        cl_log.log( QString::fromAscii("Failed to switch camera %1 relay output %2 to monostable mode. %3").
            arg(QString::fromAscii(soapWrapper.endpoint())).arg(QString::fromStdString(relayOutputInfo.token)).arg(m_prevSoapCallResult), cl_logWARNING );
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
        cl_log.log( QString::fromAscii("Failed to get relay output %1 info").arg(outputID), cl_logWARNING );
        return false;
    }

    const bool isBistableModeRequired = autoResetTimeoutMS == 0;
    std::string requiredDelayTime;
    if( autoResetTimeoutMS > 0 )
    {
        std::ostringstream ss;
        ss<<"PT"<<(autoResetTimeoutMS < 1000 ? 1 : autoResetTimeoutMS/1000)<<"S";
        requiredDelayTime = ss.str();
    }
    if( (relayOutputInfo.isBistable != isBistableModeRequired) || 
        (!isBistableModeRequired && relayOutputInfo.delayTime != requiredDelayTime) ||
        relayOutputInfo.activeByDefault )
    {
        //switching output to required mode
        relayOutputInfo.isBistable = isBistableModeRequired;
        relayOutputInfo.delayTime = requiredDelayTime;
        relayOutputInfo.activeByDefault = false;
        if( !setRelayOutputSettings( relayOutputInfo ) )
        {
            cl_log.log( QString::fromAscii("Cannot set camera %1 output %2 to state %3 with timeout %4 msec. Cannot set mode to %5. %6").
                arg(QString()).arg(QString::fromStdString(relayOutputInfo.token)).arg(QLatin1String(active ? "active" : "inactive")).arg(autoResetTimeoutMS).
                arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")).arg(m_prevSoapCallResult), cl_logWARNING );
            return false;
        }

        cl_log.log( QString::fromAscii("Camera %1 output %2 has been switched to %3 mode").arg(QString()).arg(outputID).
            arg(QLatin1String(relayOutputInfo.isBistable ? "bistable" : "monostable")), cl_logWARNING );
    }

    //modifying output
    const QAuthenticator& auth = getAuth();
    DeviceSoapWrapper soapWrapper(
        getDeviceOnvifUrl().toStdString(),
        auth.user().toStdString(),
        auth.password().toStdString(),
        m_timeDrift );

    _onvifDevice__SetRelayOutputState request;
    request.RelayOutputToken = relayOutputInfo.token;
    request.LogicalState = active ? onvifXsd__RelayLogicalState__active : onvifXsd__RelayLogicalState__inactive;
    _onvifDevice__SetRelayOutputStateResponse response;
    m_prevSoapCallResult = soapWrapper.setRelayOutputState( request, response );
    if( m_prevSoapCallResult != SOAP_OK && m_prevSoapCallResult != SOAP_MUSTUNDERSTAND )
    {
        cl_log.log( QString::fromAscii("Failed to set relay %1 output state to %2. endpoint %3").
            arg(QString::fromStdString(relayOutputInfo.token)).arg(active).arg(QString::fromAscii(soapWrapper.endpoint())), cl_logWARNING );
        return false;
    }

    cl_log.log( QString::fromAscii("Successfully set relay %1 output state to %2. endpoint %3").
        arg(QString::fromStdString(relayOutputInfo.token)).arg(active).arg(QString::fromAscii(soapWrapper.endpoint())), cl_logWARNING );
    return true;
}
