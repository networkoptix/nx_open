#include <climits>
#include <QDebug>
#include <QHash>
#include <cmath>

#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "onvif_helper.h"
#include "utils/common/synctime.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapMediaBindingProxy.h"
#include "api/app_server_connection.h"

const char* QnPlOnvifResource::MANUFACTURE = "OnvifDevice";
static const float MAX_EPS = 0.01f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifResource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifResource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifResource::DEFAULT_IFRAME_DISTANCE = 20;
const QString& QnPlOnvifResource::MEDIA_URL_PARAM_NAME = *(new QString("MediaUrl"));
const QString& QnPlOnvifResource::DEVICE_URL_PARAM_NAME = *(new QString("DeviceUrl"));
const float QnPlOnvifResource::QUALITY_COEF = 0.2f;
const char* QnPlOnvifResource::PROFILE_NAME_PRIMARY = "Netoptix Primary";
const char* QnPlOnvifResource::PROFILE_NAME_SECONDARY = "Netoptix Secondary";
const int QnPlOnvifResource::MAX_AUDIO_BITRATE = 64; //kbps
const int QnPlOnvifResource::MAX_AUDIO_SAMPLERATE = 32; //khz

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

//
// QnPlOnvifResource
//

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
                if (senderIpAddress == conf->FromDHCP->Address.c_str()) {
                    return QString(ifacePtr->Info->HwAddress.c_str()).toUpper().replace(":", "-");
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString(ifacePtr->Info->HwAddress.c_str());
                }
            }

            std::vector<class onvifXsd__PrefixedIPv4Address*> addresses = ifacePtr->IPv4->Config->Manual;
            std::vector<class onvifXsd__PrefixedIPv4Address*>::const_iterator addrPtrIter = addresses.begin();

            while (addrPtrIter != addresses.end()) {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;

                if (senderIpAddress == addrPtr->Address.c_str()) {
                    return QString(ifacePtr->Info->HwAddress.c_str()).toUpper().replace(":", "-");
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString(ifacePtr->Info->HwAddress.c_str());
                }

                ++addrPtrIter;
            }
        }

        ++ifacePtrIter;
    }

    return someMacAddress.toUpper().replace(":", "-");
}

const QString QnPlOnvifResource::createOnvifEndpointUrl(const QString& ipAddress) {
    return ONVIF_PROTOCOL_PREFIX + ipAddress + ONVIF_URL_SUFFIX;
}

QnPlOnvifResource::QnPlOnvifResource() :
    m_maxFps(QnPhysicalCameraResource::getMaxFps()),
    m_iframeDistance(DEFAULT_IFRAME_DISTANCE),
    m_minQuality(0),
    m_maxQuality(0),
    m_hasDual(false),
    m_mediaUrl(),
    m_deviceOnvifUrl(),
    m_reinitDeviceInfo(false),
    m_codec(H264),
    m_audioCodec(AUDIO_NONE),
    m_primaryResolution(EMPTY_RESOLUTION_PAIR),
    m_secondaryResolution(EMPTY_RESOLUTION_PAIR),
    m_audioBitrate(0),
    m_audioSamplerate(0)
{
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
    return MANUFACTURE;
}

bool QnPlOnvifResource::hasDualStreaming() const
{
    QMutexLocker lock(&m_mutex);
    return m_hasDual;
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

QnPlOnvifResource::CODECS QnPlOnvifResource::getCodec() const
{
    QMutexLocker lock(&m_mutex);
    return m_codec;
}

void QnPlOnvifResource::setCodec(QnPlOnvifResource::CODECS c)
{
    QMutexLocker lock(&m_mutex);
    m_codec = c;
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
    setCodec(H264);

    setOnvifUrls();

    if (!isSoapAuthorized()) 
    {
        m_reinitDeviceInfo = true;
        setStatus(QnResource::Unauthorized);
        return false;
    }

    if (m_reinitDeviceInfo && fetchAndSetDeviceInformation())
    {
        setOnvifUrls();
        m_reinitDeviceInfo = false;
    }

    if (!fetchAndSetResourceOptions()) 
    {
        return false;
    }

    save();

    return true;
}

ResolutionPair QnPlOnvifResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolutionList.isEmpty()? EMPTY_RESOLUTION_PAIR: m_resolutionList.front();
}

float QnPlOnvifResource::getResolutionAspectRatio(const ResolutionPair& resolution) const
{
    return resolution.second == 0? 0: static_cast<double>(resolution.first) / resolution.second;
}

ResolutionPair QnPlOnvifResource::getNearestResolutionForSecondary(const ResolutionPair& resolution, float aspectRatio) const
{
    return getNearestResolution(resolution, aspectRatio, MAX_SECONDARY_RESOLUTION_SQUARE);
}

ResolutionPair QnPlOnvifResource::getNearestResolution(const ResolutionPair& resolution, float aspectRatio,
    double maxResolutionSquare) const
{
    QMutexLocker lock(&m_mutex);

    int bestIndex = -1;
    double bestMatchCoeff = maxResolutionSquare > MAX_EPS? maxResolutionSquare: INT_MAX;
    double requestSquare = resolution.first * resolution.second;
    if (requestSquare == 0) return EMPTY_RESOLUTION_PAIR;

    for (int i = 0; i < m_resolutionList.size(); ++i) {
        float ar = getResolutionAspectRatio(m_resolutionList[i]);
        if (qAbs(ar - aspectRatio) > MAX_EPS) {
            continue;
        }

        double square = m_resolutionList[i].first * m_resolutionList[i].second;
        if (square == 0) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + MAX_EPS) {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }

    return bestIndex >= 0 ? m_resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

void QnPlOnvifResource::fetchAndSetPrimarySecondaryResolution()
{
    QMutexLocker lock(&m_mutex);

    if (m_resolutionList.isEmpty()) {
        return;
    }

    m_primaryResolution = m_resolutionList.front();
    float currentAspect = getResolutionAspectRatio(m_primaryResolution);
    m_secondaryResolution = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect);

    if (m_secondaryResolution != EMPTY_RESOLUTION_PAIR) {
        return;
    }

    double currentSquare = m_primaryResolution.first * m_primaryResolution.second;

    foreach (ResolutionPair resolution, m_resolutionList) {
        float aspect = getResolutionAspectRatio(resolution);
        if (abs(aspect - currentAspect) < MAX_EPS) {
            continue;
        }
        currentAspect = aspect;

        double square = resolution.first * resolution.second;
        if (square == 0 || currentSquare / square > 2.0) {
            break;
        }

        ResolutionPair tmp = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect);
        if (tmp != EMPTY_RESOLUTION_PAIR) {
            m_primaryResolution = resolution;
            m_secondaryResolution = tmp;
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

int QnPlOnvifResource::getMaxFps()
{
    //Synchronization is not needed
    return m_maxFps;
}

QString QnPlOnvifResource::getDeviceOnvifUrl() const 
{ 
    QMutexLocker lock(&m_mutex);
    return m_deviceOnvifUrl; 
}
void QnPlOnvifResource::setDeviceOnvifUrl(const QString& src) 
{ 
    QMutexLocker lock(&m_mutex);
    m_deviceOnvifUrl = src; 
}


bool QnPlOnvifResource::fetchAndSetDeviceInformation()
{
    bool result = true;
    QAuthenticator auth(getAuth());
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString());
    
    //Trying to get name
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
            setName((response.Manufacturer + " - " + response.Model).c_str());
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
            result = false;
        }

        if (response.Capabilities) 
        {
            if (response.Capabilities->Media) 
            {
                setMediaUrl(response.Capabilities->Media->XAddr.c_str());
                setParam(MEDIA_URL_PARAM_NAME, getMediaUrl(), QnDomainDatabase);
            }
            if (response.Capabilities->Device) 
            {
                setDeviceOnvifUrl(response.Capabilities->Device->XAddr.c_str());
                setParam(DEVICE_URL_PARAM_NAME, getDeviceOnvifUrl(), QnDomainDatabase);
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
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString());

    if (!fetchAndSetVideoEncoderOptions(soapWrapper)) {
        return false;
    }

    //If failed - ignore
    fetchAndSetAudioEncoderOptions(soapWrapper);
    fetchAndSetVideoSourceOptions(soapWrapper);

    updateResourceCapabilities();

    //All VideoEncoder options are set, so we can calculate resolutions for the streams
    fetchAndSetPrimarySecondaryResolution();

    //Before invoking <fetchAndSetHasDualStreaming> Primary and Secondary Resolutions MUST be set
    fetchAndSetDualStreaming(soapWrapper);
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

    if (getCodec() == H264) 
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
        QMutexLocker lock(&m_mutex);
        m_maxFps = response.Options->H264->FrameRateRange->Max;

        qDebug() << "ONVIF max FPS: " << m_maxFps;
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
    if (cl_log.logLevel() >= cl_logDEBUG1) {
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
        QMutexLocker lock(&m_mutex);
        m_maxFps = response.Options->JPEG->FrameRateRange->Max;

        qDebug() << "ONVIF max FPS: " << m_maxFps;
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
    if (cl_log.logLevel() >= cl_logDEBUG1) {
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

bool QnPlOnvifResource::isSoapAuthorized() const 
{
    QAuthenticator auth(getAuth());
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString());

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

QString QnPlOnvifResource::getMediaUrl() const 
{ 
    QMutexLocker lock(&m_mutex);
    return m_mediaUrl; 
}

void QnPlOnvifResource::setMediaUrl(const QString& src) 
{
    QMutexLocker lock(&m_mutex);
    m_mediaUrl = src; 
}


void QnPlOnvifResource::setOnvifUrls()
{
    if (getDeviceOnvifUrl().isEmpty()) {
        QVariant deviceVariant;
        bool res = getParam(DEVICE_URL_PARAM_NAME, deviceVariant, QnDomainDatabase);
        QString deviceStr(res? deviceVariant.toString(): "");

        setDeviceOnvifUrl(deviceStr.isEmpty()? createOnvifEndpointUrl(): deviceStr);

        qDebug() << "QnPlOnvifResource::setOnvifUrls: m_deviceOnvifUrl = " << getDeviceOnvifUrl();
    }

    if (getMediaUrl().isEmpty()) {
        QVariant mediaVariant;
        bool res = getParam(MEDIA_URL_PARAM_NAME, mediaVariant, QnDomainDatabase);
        QString mediaStr(res? mediaVariant.toString(): "");

        setMediaUrl(mediaStr.isEmpty()? createOnvifEndpointUrl(): mediaStr);

        qDebug() << "QnPlOnvifResource::setOnvifUrls: m_mediaUrl = " << getMediaUrl();
    }
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

bool QnPlOnvifResource::fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper)
{
    VideoOptionsReq request;
    request.ConfigurationToken = NULL;
    request.ProfileToken = NULL;

    VideoOptionsResp response;

    int soapRes = soapWrapper.getVideoEncoderConfigurationOptions(request, response);
    if (soapRes != SOAP_OK || !response.Options) {

        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't receive data from camera (or data is empty) (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;

    }

    if (response.Options->H264) 
    {
        setCodec(H264);
    } 
    else if (response.Options->JPEG) 
    {
        setCodec(JPEG);
    } 
    else 
    {

        qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: camera didn't return data for H264 and MJPEG (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError();
        return false;

    }

    setVideoEncoderOptions(response);

    return true;
}

bool QnPlOnvifResource::fetchAndSetDualStreaming(MediaSoapWrapper& soapWrapper)
{
    VideoConfigsReq request;
    VideoConfigsResp response;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnPlOnvifResource::fetchAndSetDualStreaming: can't define availability of dual streaming (URL: "
            << getMediaUrl() << ", UniqueId: " << getUniqueId() << "). Root cause: SOAP request failed. GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_hasDual = m_secondaryResolution != EMPTY_RESOLUTION_PAIR && response.Configurations.size() > 1;
    }

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
            ceil == *it;
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
