#include <climits>
#include <QDebug>
#include <QHash>
#include <cmath>

#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "onvif_helper.h"
#include "utils/common/synctime.h"
#include "utils/common/math.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapMediaBindingProxy.h"
#include "api/app_server_connection.h"

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
const char* QnPlOnvifResource::PROFILE_NAME_PRIMARY = "Netoptix Primary";
const char* QnPlOnvifResource::PROFILE_NAME_SECONDARY = "Netoptix Secondary";
const int QnPlOnvifResource::MAX_AUDIO_BITRATE = 64; //kbps
const int QnPlOnvifResource::MAX_AUDIO_SAMPLERATE = 32; //khz
const int QnPlOnvifResource::ADVANCED_SETTINGS_VALID_TIME = 200; //200 ms

//Forth times greater than default = 320 x 240
const double QnPlOnvifResource::MAX_SECONDARY_RESOLUTION_SQUARE =
    SECONDARY_STREAM_DEFAULT_RESOLUTION.first * SECONDARY_STREAM_DEFAULT_RESOLUTION.second * 4;


//width > height is prefered
bool resolutionGreaterThan(const ResolutionPair &s1, const ResolutionPair &s2)
{
    long long res1 = s1.first * s1.second;
    long long res2 = s2.first * s2.second;
    return res1 > res2? true: (res1 == res2 && s1.first > s2.first? true: false);
}

struct VideoOptionsLocal
{
    QString id;
    VideoOptionsResp optionsResp;

    VideoOptionsLocal() {}
};

bool videoOptsGreaterThan(const VideoOptionsLocal &s1, const VideoOptionsLocal &s2)
{
    if (!s1.optionsResp.Options || !s2.optionsResp.Options)
        return s1.optionsResp.Options > s2.optionsResp.Options;

    typedef std::vector<onvifXsd__VideoResolution*> ResVector;

    const ResVector& resolutionsAvailableS1 = (s1.optionsResp.Options->H264 ? s1.optionsResp.Options->H264->ResolutionsAvailable:
        (s1.optionsResp.Options->JPEG ? s1.optionsResp.Options->JPEG->ResolutionsAvailable: ResVector()));
    const ResVector& resolutionsAvailableS2 = (s2.optionsResp.Options->H264 ? s2.optionsResp.Options->H264->ResolutionsAvailable:
        (s2.optionsResp.Options->JPEG ? s2.optionsResp.Options->JPEG->ResolutionsAvailable: ResVector()));

    int square1 = 0;
    for (int i = 0; i < resolutionsAvailableS1.size(); ++i) {
        if (resolutionsAvailableS1[i])
            square1 = qMax(square1, resolutionsAvailableS1[i]->Width * resolutionsAvailableS1[i]->Height);
    }
    
    int square2 = 0;
    for (int i = 0; i < resolutionsAvailableS2.size(); ++i) {
        if (resolutionsAvailableS2[i])
            square2 = qMax(square2, resolutionsAvailableS2[i]->Width * resolutionsAvailableS2[i]->Height);
    }

    if (square1 != square2)
        return square1 > square2;

    return s1.optionsResp.Options->H264 > s2.optionsResp.Options->H264; // if some option doesn't have H264 it "less"
}

//
// QnPlOnvifResource
//

QnPlOnvifResource::QnPlOnvifResource() :
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
    m_forceCodecFromPrimaryEncoder(false),
    m_onvifAdditionalSettings(0),
    m_timeDrift(0),
    m_ptzController(0)
{
}

QnPlOnvifResource::~QnPlOnvifResource() {
    delete m_onvifAdditionalSettings;
    delete m_ptzController;
}


const QString QnPlOnvifResource::fetchMacAddress(const NetIfacesResp& response,
    const QString& senderIpAddress)
{
    QString someMacAddress;
    std::vector<class onvifXsd__NetworkInterface*> ifaces = response.NetworkInterfaces;
    std::vector<class onvifXsd__NetworkInterface*>::const_iterator ifacePtrIter = ifaces.begin();

    while (ifacePtrIter != ifaces.end()) 
    {
        onvifXsd__NetworkInterface* ifacePtr = *ifacePtrIter;

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

        ++ifacePtrIter;
    }

    return someMacAddress.toUpper().replace(QLatin1Char(':'), QLatin1Char('-'));
}

bool QnPlOnvifResource::setHostAddress(const QHostAddress &ip, QnDomain domain)
{
    //QnPhysicalCameraResource::se
    {
        QMutexLocker lock(&m_mutex);

        QString mediaUrl = getMediaUrl();
        if (!mediaUrl.isEmpty())
        {
            QUrl url(mediaUrl);
            url.setHost(ip.toString());
            setMediaUrl(url.toString());
        }

        QString onvifUrl = getDeviceOnvifUrl();
        if (!onvifUrl.isEmpty())
        {
            QUrl url(onvifUrl);
            url.setHost(ip.toString());
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

bool QnPlOnvifResource::updateMACAddress()
{
    return true;
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

const CameraPhysicalWindowSize QnPlOnvifResource::getPhysicalWindowSize() const
{
    QMutexLocker lock(&m_mutex);
    return m_physicalWindowSize;
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

    m_timeDrift = calcTimeDrift();
    /*
    if (!isSoapAuthorized()) 
    {
        setStatus(QnResource::Unauthorized);
        return false;
    }
    */

    if (getImagingUrl().isEmpty() || getMediaUrl().isEmpty() || getName().contains(QLatin1String("Unknown")) || getMAC().isEmpty() || m_needUpdateOnvifUrl)
    {
        if (!fetchAndSetDeviceInformation() && getMediaUrl().isEmpty())
        {
            qCritical() << "QnPlOnvifResource::initInternal: ONVIF media url is absent. Id: " << getPhysicalId();
            return false;
        }
        else
            m_needUpdateOnvifUrl = false;

        
    }

    if (!fetchAndSetResourceOptions()) 
    {
        m_needUpdateOnvifUrl = true;
        return false;
    }

    if (!hasDualStreaming())
        setMotionType(MT_NoMotion);

    //if (getStatus() == QnResource::Offline || getStatus() == QnResource::Unauthorized)
    //    setStatus(QnResource::Online, true); // to avoid infinit status loop in this version

    //Additional camera settings
    fetchAndSetCameraSettings();

    if (m_ptzController)
        addCameraCapabilities(HasPtzMove | HasPtzZoom);

    save();

    return true;
}

ResolutionPair QnPlOnvifResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolutionList.isEmpty()? EMPTY_RESOLUTION_PAIR: m_resolutionList.front();
}

float QnPlOnvifResource::getResolutionAspectRatio(const ResolutionPair& resolution)
{
    return resolution.second == 0? 0: static_cast<double>(resolution.first) / resolution.second;
}

ResolutionPair QnPlOnvifResource::getNearestResolutionForSecondary(const ResolutionPair& resolution, float aspectRatio) const
{
    QMutexLocker lock(&m_mutex);
    return getNearestResolution(resolution, aspectRatio, MAX_SECONDARY_RESOLUTION_SQUARE, m_secondaryResolutionList);
}

ResolutionPair QnPlOnvifResource::getNearestResolution(const ResolutionPair& resolution, float aspectRatio,
    double maxResolutionSquare, const QList<ResolutionPair>& resolutionList)
{
    double requestSquare = resolution.first * resolution.second;
    if (requestSquare < MAX_EPS || requestSquare > maxResolutionSquare) return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff = maxResolutionSquare > MAX_EPS ? (maxResolutionSquare / requestSquare) : INT_MAX;

    for (int i = 0; i < resolutionList.size(); ++i) {
        ResolutionPair tmp;

        tmp.first = qPower2Ceil(static_cast<unsigned int>(resolutionList[i].first + 1), 8);
        tmp.second = qPower2Floor(static_cast<unsigned int>(resolutionList[i].second - 1), 8);
        float ar1 = getResolutionAspectRatio(tmp);

        tmp.first = qPower2Floor(static_cast<unsigned int>(resolutionList[i].first - 1), 8);
        tmp.second = qPower2Ceil(static_cast<unsigned int>(resolutionList[i].second + 1), 8);
        float ar2 = getResolutionAspectRatio(tmp);

        if (aspectRatio != 0 && !qBetween(aspectRatio, qMin(ar1,ar2), qMax(ar1,ar2)))
        {
            continue;
        }

        double square = resolutionList[i].first * resolutionList[i].second;
        if (square < MAX_EPS) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + MAX_EPS) {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }

    return bestIndex >= 0 ? resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

void QnPlOnvifResource::fetchAndSetPrimarySecondaryResolution()
{
    QMutexLocker lock(&m_mutex);

    if (m_resolutionList.isEmpty()) {
        return;
    }

    m_primaryResolution = m_resolutionList.front();
    float currentAspect = getResolutionAspectRatio(m_primaryResolution);

    // SD NTCS/PAL resolutions have non standart SAR. fix it
    if (m_primaryResolution.first == 720 && (m_primaryResolution.second == 480 || m_primaryResolution.second == 576))
    {
        currentAspect = float(4.0 / 3.0);
    }
    m_secondaryResolution = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect);
    if (m_secondaryResolution == EMPTY_RESOLUTION_PAIR)
        m_secondaryResolution = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0); // try to get resolution ignoring aspect ration

    if (m_secondaryResolution != EMPTY_RESOLUTION_PAIR) {
        Q_ASSERT(m_secondaryResolution.first <= SECONDARY_STREAM_MAX_RESOLUTION.first &&
            m_secondaryResolution.second <= SECONDARY_STREAM_MAX_RESOLUTION.second);
        return;
    }

    double maxSquare = m_primaryResolution.first * m_primaryResolution.second;

    foreach (ResolutionPair resolution, m_resolutionList) {
        float aspect = getResolutionAspectRatio(resolution);
        if (abs(aspect - currentAspect) < MAX_EPS) {
            continue;
        }
        currentAspect = aspect;

        double square = resolution.first * resolution.second;
        if (square <= 0.90 * maxSquare) {
            break;
        }

        ResolutionPair tmp = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect);
        if (tmp != EMPTY_RESOLUTION_PAIR) {
            m_primaryResolution = resolution;
            m_secondaryResolution = tmp;

            Q_ASSERT(m_secondaryResolution.first <= SECONDARY_STREAM_MAX_RESOLUTION.first &&
                m_secondaryResolution.second <= SECONDARY_STREAM_MAX_RESOLUTION.second);

            return;
        }
    }
}

ResolutionPair QnPlOnvifResource::getPrimaryResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_primaryResolution;
}

ResolutionPair QnPlOnvifResource::getSecondaryResolution() const
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

bool QnPlOnvifResource::fetchAndSetDeviceInformation()
{
    bool result = true;
    QAuthenticator auth(getAuth());
    //TODO:UTF unuse StdString
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    QString user = auth.user();
    QString password = auth.password();
    
    //Trying to get name
    /*
    {
        DeviceInfoReq request;
        DeviceInfoResp response;

        int soapRes = soapWrapper.getDeviceInformation(request, response);
        if (soapRes != SOAP_OK) 
        {
            qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
                << soapWrapper.getEndpointUrl() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastError();

            result = false;
        } 
        else
        {
            // TODO: Sergey G. Don't change camera name! User is possible changed it already by hand.
            // setName((response.Manufacturer + " - " + response.Model).c_str());
        }
    }
    */

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
            result = false;
        }

        if (response.Capabilities) 
        {
            //TODO:UTF unuse std::string
            if (response.Capabilities->Media) 
            {
                setMediaUrl(QString::fromStdString(response.Capabilities->Media->XAddr));
            }
            if (response.Capabilities->Imaging)
            {
                setImagingUrl(QString::fromStdString(response.Capabilities->Imaging->XAddr));
            }
            if (response.Capabilities->Device) 
            {
                setDeviceOnvifUrl(QString::fromStdString(response.Capabilities->Device->XAddr));
            }
            if (response.Capabilities->PTZ) 
            {
                setPtzfUrl(QString::fromStdString(response.Capabilities->PTZ->XAddr));
            }
        }
    }

    //Trying to get MAC
    {
        NetIfacesReq request;
        NetIfacesResp response;

        int soapRes = soapWrapper.getNetworkInterfaces(request, response);
        if (soapRes != SOAP_OK) 
        {
            qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch MAC address. Reason: SOAP to endpoint "
                << getDeviceOnvifUrl() << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
            result = false;
        }

        QString mac = fetchMacAddress(response, QUrl(getDeviceOnvifUrl()).host());

        if (!mac.isEmpty()) 
            setMAC(mac);

    }

    return result;
}

bool QnPlOnvifResource::fetchAndSetResourceOptions()
{
    QAuthenticator auth(getAuth());
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_timeDrift);

    if (!fetchAndSetVideoEncoderOptions(soapWrapper)) {
        return false;
    }

    //If failed - ignore
    if (fetchAndSetAudioEncoderOptions(soapWrapper)) 
    {
        setParam(AUDIO_SUPPORTED_PARAM_NAME, 1, QnDomainDatabase);
    }
    else 
    {
        setParam(AUDIO_SUPPORTED_PARAM_NAME, 0, QnDomainDatabase);
    }

    fetchAndSetVideoSourceOptions(soapWrapper);

    if (!updateResourceCapabilities()) {
        return false;
    }

    //All VideoEncoder options are set, so we can calculate resolutions for the streams
    fetchAndSetPrimarySecondaryResolution();

    //Before invoking <fetchAndSetHasDualStreaming> Primary and Secondary Resolutions MUST be set
    fetchAndSetDualStreaming(soapWrapper);

    fetchAndSetAudioEncoder(soapWrapper);
    fetchAndSetVideoSource(soapWrapper);
    fetchAndSetAudioSource(soapWrapper);

    return true;
}

void QnPlOnvifResource::updateSecondaryResolutionList(const VideoOptionsResp& response)
{
    if (!response.Options)
        return;
    const std::vector<onvifXsd__VideoResolution*>* resolutionsPtr = 0;
    if (getCodec(false) == H264 && response.Options->H264)
        resolutionsPtr = &response.Options->H264->ResolutionsAvailable;
    else if (response.Options->JPEG)
        resolutionsPtr = &response.Options->JPEG->ResolutionsAvailable;
    if (!resolutionsPtr)
        return;
    const std::vector<onvifXsd__VideoResolution*>& resolutions = *resolutionsPtr;

    QMutexLocker lock(&m_mutex);
    m_secondaryResolutionList.clear();
    for (unsigned int i = 0; i < resolutions.size(); ++i)
        m_secondaryResolutionList.append(ResolutionPair(resolutions[i]->Width, resolutions[i]->Height));
    qSort(m_secondaryResolutionList.begin(), m_secondaryResolutionList.end(), resolutionGreaterThan);
}

void QnPlOnvifResource::setVideoEncoderOptions(const VideoOptionsResp& response) {
    if (!response.Options) {
        return;
    }

    if (response.Options->QualityRange) {
        setMinMaxQuality(response.Options->QualityRange->Min, response.Options->QualityRange->Max);

        qDebug() << "ONVIF quality range [" << m_minQuality << ", " << m_maxQuality << "]";
    } else {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: camera didn't return quality range. UniqueId: " << getUniqueId();
    }

    if (response.Options->H264) 
    {
        setVideoEncoderOptionsH264(response);
    } 
    else 
    {
        setVideoEncoderOptionsJpeg(response);
    }
}

void QnPlOnvifResource::setVideoEncoderOptionsH264(const VideoOptionsResp& response) {
    if (!response.Options->H264) {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: Max FPS, Iframe Distance and Resolution List. UniqueId: " << getUniqueId();
        return;
    }

    if (response.Options->H264->FrameRateRange) 
    {
        setMaxFps(response.Options->H264->FrameRateRange->Max);
    } 
    else 
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Max FPS. UniqueId: " << getUniqueId();
    }

    if (response.Options->H264->GovLengthRange) 
    {
        QMutexLocker lock(&m_mutex);
        m_iframeDistance = DEFAULT_IFRAME_DISTANCE <= response.Options->H264->GovLengthRange->Min? response.Options->H264->GovLengthRange->Min:
            (DEFAULT_IFRAME_DISTANCE >= response.Options->H264->GovLengthRange->Max? response.Options->H264->GovLengthRange->Max: DEFAULT_IFRAME_DISTANCE);
        qDebug() << "ONVIF iframe distance: " << m_iframeDistance;

    } 
    else 
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Iframe Distance. UniqueId: " << getUniqueId();
    }

    std::vector<onvifXsd__VideoResolution*>& resolutions = response.Options->H264->ResolutionsAvailable;
    if (resolutions.size() == 0) {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsH264: can't fetch Resolutions. UniqueId: " << getUniqueId();
    } 
    else 
    {
        QMutexLocker lock(&m_mutex);
        m_resolutionList.clear();

        std::vector<onvifXsd__VideoResolution*>::const_iterator resPtrIter = resolutions.begin();
        while (resPtrIter != resolutions.end()) 
        {
            m_resolutionList.append(ResolutionPair((*resPtrIter)->Width, (*resPtrIter)->Height));
            ++resPtrIter;
        }
        qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);

    }

    QMutexLocker lock(&m_mutex);

    //Printing fetched resolutions
    if (cl_log.logLevel() > cl_logDEBUG1) {
        qDebug() << "ONVIF resolutions: ";
        foreach (ResolutionPair resolution, m_resolutionList) {
            qDebug() << resolution.first << " x " << resolution.second;
        }
    }
}

void QnPlOnvifResource::setVideoEncoderOptionsJpeg(const VideoOptionsResp& response) 
{

    if (!response.Options->JPEG) 
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Max FPS, Iframe Distance and Resolution List. UniqueId: " << getUniqueId();
    }

    if (response.Options->JPEG->FrameRateRange) 
    {
        setMaxFps(response.Options->JPEG->FrameRateRange->Max);
    } 
    else 
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Max FPS. UniqueId: " << getUniqueId();
    }

    std::vector<onvifXsd__VideoResolution*>& resolutions = response.Options->JPEG->ResolutionsAvailable;
    if (resolutions.size() == 0) 
    {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptionsJpeg: can't fetch Resolutions. UniqueId: " << getUniqueId();
    } 
    else 
    {
        QMutexLocker lock(&m_mutex);

        m_resolutionList.clear();

        std::vector<onvifXsd__VideoResolution*>::const_iterator resPtrIter = resolutions.begin();
        while (resPtrIter != resolutions.end()) {
            m_resolutionList.append(ResolutionPair((*resPtrIter)->Width, (*resPtrIter)->Height));
            ++resPtrIter;
        }
        qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);
    }

    QMutexLocker lock(&m_mutex);
    //Printing fetched resolutions
    if (cl_log.logLevel() > cl_logDEBUG1) {
        qDebug() << "ONVIF resolutions: ";
        foreach (ResolutionPair resolution, m_resolutionList) {
            qDebug() << resolution.first << " x " << resolution.second;
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

int QnPlOnvifResource::calcTimeDrift() const 
{
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), "", "", 0);

    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = soapWrapper.GetSystemDateAndTime(request, response);

    if (soapRes == SOAP_OK && response.SystemDateAndTime->UTCDateTime)
    {
        onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
        onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;

        QDateTime datetime(QDate(date->Year, date->Month, date->Day), QTime(time->Hour, time->Minute, time->Second), Qt::UTC);
        int drift = datetime.toMSecsSinceEpoch()/1000 - QDateTime::currentMSecsSinceEpoch()/1000;
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

void QnPlOnvifResource::save()
{
    QByteArray errorStr;
    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>(), errorStr) != 0) {
        qCritical() << "QnPlOnvifResource::init: can't save resource params to Enterprise Controller. Resource physicalId: "
                    << getPhysicalId() << ". Description: " << errorStr;
    }
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

bool QnPlOnvifResource::mergeResourcesIfNeeded(QnNetworkResourcePtr source)
{
    QnPlOnvifResourcePtr onvifR = source.dynamicCast<QnPlOnvifResource>();
    if (!onvifR)
        return false;

    QString onvifUrlSource = onvifR->getDeviceOnvifUrl();
    QString mediaUrlSource = onvifR->getMediaUrl();

    bool result = false;

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
        newUrl.setHost(getHostAddress().toString());
        setDeviceOnvifUrl(newUrl.toString());
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << getDeviceOnvifUrl();
        result = true;
    }

    if (getMediaUrl().size() != 0 && QUrl(getMediaUrl()).host().size()==0)
    {
        // trying to introduce fix for dw cam 
        QString temp = getMediaUrl();
        QUrl newUrl(getMediaUrl());
        newUrl.setHost(getHostAddress().toString());
        setMediaUrl(newUrl.toString());
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << getMediaUrl();
        result = true;
    }


    return result;
}

int  QnPlOnvifResource::getH264StreamProfile(const VideoOptionsResp& response)
{
    if (!response.Options || !response.Options->H264)
        return -1; // inform do not change profile

    QVector<onvifXsd__H264Profile> profiles = QVector<onvifXsd__H264Profile>::fromStdVector(response.Options->H264->H264ProfilesSupported);
    if (profiles.isEmpty())
        return -1;
    qSort(profiles);
    return (int) profiles[0];
}

bool QnPlOnvifResource::fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper)
{
    VideoConfigsReq confRequest;
    VideoConfigsResp confResponse;

    m_forceCodecFromPrimaryEncoder = false;
    int soapRes = soapWrapper.getVideoEncoderConfigurations(confRequest, confResponse); // get encoder list
    if (soapRes != SOAP_OK) {
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't get list of video encoders from camera (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << "). GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    std::string login = soapWrapper.getLogin()? soapWrapper.getLogin() : "";
    std::string password = soapWrapper.getPassword()? soapWrapper.getPassword() : "";
    std::string endpoint = soapWrapper.getEndpointUrl().toStdString();

    VideoOptionsReq optRequest;

    QList<VideoOptionsLocal> optionsList;
    QList<MediaSoapWrapperPtr> soapWrappersList;

    std::vector<onvifXsd__VideoEncoderConfiguration*>::const_iterator encIt = confResponse.Configurations.begin();
    for (;encIt != confResponse.Configurations.end(); ++encIt)
    {
        if (!*encIt) 
            continue;

        onvifXsd__VideoEncoderConfiguration* configuration = *encIt;

        MediaSoapWrapperPtr soapWrapperPtr(new MediaSoapWrapper(endpoint, login, password, m_timeDrift));
        soapWrappersList.append(soapWrapperPtr);

        //qWarning() << "camera" << soapWrapperPtr->getEndpointUrl() << "get params from configuration" << configuration->Name.c_str();

        optionsList.append(VideoOptionsLocal());
        VideoOptionsLocal& currVideoOpts = optionsList.back();

        optRequest.ConfigurationToken = &(*encIt)->token;
        optRequest.ProfileToken = NULL;

        int retryCount = getMaxOnvifRequestTries();
        soapRes = SOAP_ERR;

        for (;soapRes != SOAP_OK && retryCount >= 0; --retryCount)
        {
            soapRes = soapWrapperPtr->getVideoEncoderConfigurationOptions(optRequest, currVideoOpts.optionsResp); // get options per encoder
            if (soapRes != SOAP_OK || !currVideoOpts.optionsResp.Options) {

                qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't receive options (or data is empty) for video encoder '" 
                    << QString::fromStdString(*(optRequest.ConfigurationToken)) << "' from camera (URL: "  << soapWrapperPtr->getEndpointUrl() << ", UniqueId: " << getUniqueId()
                    << "). Root cause: SOAP request failed. GSoap error code: " << soapRes << ". " << soapWrapperPtr->getLastError();
                //return false;
            }

            else if (!currVideoOpts.optionsResp.Options->H264 && !currVideoOpts.optionsResp.Options->JPEG)
            {
                qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: video encoder '" << optRequest.ConfigurationToken->c_str()
                    << "' contains no data for H264/JPEG (URL: "  << soapWrapperPtr->getEndpointUrl() << ", UniqueId: " << getUniqueId() << ")." << "Ignoring and use default codec list";
                if (!optionsList.isEmpty())
                {
                    // no codec info for secondary encoder
                    m_forceCodecFromPrimaryEncoder = true;
                }
            }
        }


        
        if (soapRes != SOAP_OK) {
            qWarning() << "camera" << soapWrapperPtr->getEndpointUrl() << "got soap error for configuration" << configuration->Name.c_str() << "skip configuration";
            optionsList.pop_back();
            continue;
        }

        //TODO:UTF unuse std::string
        currVideoOpts.id = QString::fromStdString(*optRequest.ConfigurationToken);
    }

    qSort(optionsList.begin(), optionsList.end(), videoOptsGreaterThan);

    if (optionsList.isEmpty())
    {
        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: all video options are empty. (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId() << ").";

        return false;
    }


    if (optionsList[0].optionsResp.Options->H264) {
        m_primaryH264Profile = getH264StreamProfile(optionsList[0].optionsResp);
        setCodec(H264, true);
    }
    else if (optionsList[0].optionsResp.Options->JPEG) {
        setCodec(JPEG, true);
    }

    setVideoEncoderOptions(optionsList[0].optionsResp);
    checkMaxFps(confResponse, optionsList[0].id);

    {
        QMutexLocker lock(&m_mutex);
        m_secondaryResolutionList = m_resolutionList;
        m_primaryVideoEncoderId = optionsList[0].id;

        if (optionsList.size() > 1) 
        {
            m_secondaryVideoEncoderId = optionsList[1].id;
            if (optionsList[1].optionsResp.Options->H264) {
                m_secondaryH264Profile = getH264StreamProfile(optionsList[1].optionsResp);
                setCodec(H264, false);
            }
            else {
                setCodec(JPEG, false);
            }
            updateSecondaryResolutionList(optionsList[1].optionsResp);
        }
    }

    return true;
}

bool QnPlOnvifResource::fetchAndSetDualStreaming(MediaSoapWrapper& /*soapWrapper*/)
{
    QMutexLocker lock(&m_mutex);

    bool dualStreaming = m_secondaryResolution != EMPTY_RESOLUTION_PAIR && !m_secondaryVideoEncoderId.isEmpty();
    setParam(DUAL_STREAMING_PARAM_NAME, dualStreaming ? 1 : 0, QnDomainDatabase);
    return true;
}

bool QnPlOnvifResource::fetchAndSetAudioEncoderOptions(MediaSoapWrapper& soapWrapper)
{
    AudioOptionsReq request;
    request.ConfigurationToken = NULL;
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

bool QnPlOnvifResource::fetchAndSetVideoSourceOptions(MediaSoapWrapper& soapWrapper)
{
    VideoSrcOptionsReq request;
    request.ConfigurationToken = NULL;
    request.ProfileToken = NULL;

    VideoSrcOptionsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurationOptions(request, response);
    if (soapRes != SOAP_OK || !response.Options) {

        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSourceOptions: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;

    }

    setVideoSourceOptions(*response.Options);

    return true;
}

void QnPlOnvifResource::setVideoSourceOptions(const VideoSrcOptions& options)
{
    if (!options.BoundsRange) {
        qWarning() << "QnPlOnvifResource::setVideoSourceOptions: camera didn't return physical window size ( UniqueId: " 
            << getUniqueId() << ").";
        return;
    }

    int x = 0;
    if (options.BoundsRange->XRange)
    {
        x = options.BoundsRange->XRange->Min;
    }
    else
    {
        qWarning() << "QnPlOnvifResource::setVideoSourceOptions: camera didn't return X-range ( UniqueId: " 
            << getUniqueId() << ").";
    }

    int y = 0;
    if (options.BoundsRange->YRange)
    {
        y = options.BoundsRange->YRange->Min;
    }
    else
    {
        qWarning() << "QnPlOnvifResource::setVideoSourceOptions: camera didn't return Y-range ( UniqueId: " 
            << getUniqueId() << ").";
    }

    int width = 0;
    if (options.BoundsRange->WidthRange)
    {
        width = options.BoundsRange->WidthRange->Max;
    }
    else
    {
        qWarning() << "QnPlOnvifResource::setVideoSourceOptions: camera didn't return width-range ( UniqueId: " 
            << getUniqueId() << ").";
    }

    int height = 0;
    if (options.BoundsRange->HeightRange)
    {
        height = options.BoundsRange->HeightRange->Max;
    }
    else
    {
        qWarning() << "QnPlOnvifResource::setVideoSourceOptions: camera didn't return width-range ( UniqueId: " 
            << getUniqueId() << ").";
    }

    {
        QMutexLocker lock(&m_mutex);
        m_physicalWindowSize.x = x;
        m_physicalWindowSize.y = y;
        m_physicalWindowSize.width = width;
        m_physicalWindowSize.height = height;
    }
}

bool QnPlOnvifResource::updateResourceCapabilities()
{
    QMutexLocker lock(&m_mutex);

    if (!m_physicalWindowSize.isValid()) {
        return true;
    }

    QList<ResolutionPair>::iterator it = m_resolutionList.begin();
    while (it != m_resolutionList.end())
    {
        if (it->first > m_physicalWindowSize.width || it->second > m_physicalWindowSize.height)
        {
            it = m_resolutionList.erase(it);
        } else {
            return true;
        }
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
        onvifXsd__AudioEncoderConfiguration* conf = response.Configurations.at(0);

        if (conf) {
            QMutexLocker lock(&m_mutex);
            //TODO:UTF unuse std::string
            m_audioEncoderId = QString::fromStdString(conf->token);
        }
    }

    return true;
}

bool QnPlOnvifResource::fetchAndSetVideoSource(MediaSoapWrapper& soapWrapper)
{
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

    if (response.Configurations.empty()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetVideoSource: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;
    } else {
        onvifXsd__VideoSourceConfiguration* conf = response.Configurations.at(0);

        if (conf) {
            QMutexLocker lock(&m_mutex);
            //TODO:UTF unuse std::string
            m_videoSourceId = QString::fromStdString(conf->token);
            m_videoSourceToken = QString::fromStdString(conf->SourceToken);
        }
    }

    return true;
}

bool QnPlOnvifResource::fetchAndSetAudioSource(MediaSoapWrapper& soapWrapper)
{
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

    if (response.Configurations.empty()) {
        qWarning() << "QnPlOnvifResource::fetchAndSetAudioSource: empty data received from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;
    } else {
        onvifXsd__AudioSourceConfiguration* conf = response.Configurations.at(0);

        if (conf) {
            QMutexLocker lock(&m_mutex);
            //TODO:UTF unuse std::string
            m_audioSourceId = QString::fromStdString(conf->token);
        }
    }

    return true;
}

const QnResourceAudioLayout* QnPlOnvifResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
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

bool QnPlOnvifResource::forcePrimaryEncoderCodec() const
{
    return m_forceCodecFromPrimaryEncoder;
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
        QHash<QString, OnvifCameraSettingOperationAbstract*>::ConstIterator opIt =
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

    OnvifCameraSettingReader reader(*settings);

    reader.read() && reader.proceed();

    CameraSettings& onvifSettings = settings->getCameraSettings();
    CameraSettings::ConstIterator it = onvifSettings.begin();

    for (; it != onvifSettings.end(); ++it) {
        setParam(it.key(), it.value().serializeToStr(), QnDomainPhysical);
    }


    if (m_ptzController == 0) 
    {
        QnOnvifPtzController* controller = new QnOnvifPtzController(toSharedPointer());
        if (!controller->getPtzConfigurationToken().isEmpty())
            m_ptzController = controller;
        else
            delete controller;
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

void QnPlOnvifResource::checkMaxFps(VideoConfigsResp& response, const QString& encoderId)
{
    VideoEncoder* vEncoder = 0;
    for (int i = 0; i < response.Configurations.size(); ++i)
    {
        if (QString::fromStdString(response.Configurations[i]->token) == encoderId)
            vEncoder = response.Configurations[i];
    }
    if (!vEncoder)    
        return;

    int maxFpsOrig = getMaxFps();
    int rangeHi = getMaxFps()-2;
    int rangeLow = getMaxFps()/4;
    int currentFps = rangeHi;
    int prevFpsValue = -1;

    vEncoder->Resolution->Width = m_resolutionList[0].first;
    vEncoder->Resolution->Height = m_resolutionList[0].second;
    
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

QnOnvifPtzController* QnPlOnvifResource::getPtzController()
{
    return m_ptzController;
}
