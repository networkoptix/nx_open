#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include <climits>
#include <QHash>
#include "onvif_211_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "onvif_211_stream_reader.h"
#include "onvif_211_helper.h"
#include "utils/common/synctime.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapMediaBindingProxy.h"
#include "onvif/wsseapi.h"
#include "api/AppServerConnection.h"

const char* QnPlOnvifGeneric211Resource::MANUFACTURE = "OnvifDevice";
static const float MAX_AR_EPS = 0.01;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifGeneric211Resource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifGeneric211Resource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifGeneric211Resource::DEFAULT_IFRAME_DISTANCE = 20;
const QString& QnPlOnvifGeneric211Resource::MEDIA_URL_PARAM_NAME = *(new QString("MediaUrl"));
const QString& QnPlOnvifGeneric211Resource::DEVICE_URL_PARAM_NAME = *(new QString("DeviceUrl"));


//width > height is prefered
bool resolutionGreaterThan(const ResolutionPair &s1, const ResolutionPair &s2)
{
    long long res1 = s1.first * s1.second;
    long long res2 = s2.first * s2.second;
    return res1 > res2? true: (res1 == res2 && s1.first > s2.first? true: false);
}

struct VideoEncoders
{
    _onvifMedia__GetVideoEncoderConfigurationsResponse soapResponse;
    QHash<QString, onvifXsd__VideoEncoderConfiguration*> videoEncodersUnused;
    QHash<QString, onvifXsd__VideoEncoderConfiguration*> videoEncodersUsed;
    bool filled;
    bool soapFailed;

    VideoEncoders() { filled = false; soapFailed = false; }
};

//
// QnPlOnvifGeneric211Resource
//

const QString QnPlOnvifGeneric211Resource::createOnvifEndpointUrl(const QString& ipAddress) {
    return ONVIF_PROTOCOL_PREFIX + ipAddress + ONVIF_URL_SUFFIX;
    //return ONVIF_PROTOCOL_PREFIX + QString("174.34.67.10") + ONVIF_URL_SUFFIX;
    //return ONVIF_PROTOCOL_PREFIX + QString("174.34.67.10") + ":81/onvif/device_service";
    //return ONVIF_PROTOCOL_PREFIX + QString("174.34.67.10") + ":82/onvif/device_service";
    //return ONVIF_PROTOCOL_PREFIX + QString("174.34.67.10") + ":83/onvif/device_service";
    //return ONVIF_PROTOCOL_PREFIX + QString("192.168.2.11") + ONVIF_URL_SUFFIX;
}

QnPlOnvifGeneric211Resource::QnPlOnvifGeneric211Resource() :
    m_lastMotionReadTime(0),
    maxFps(QnPhysicalCameraResource::getMaxFps()),
    iframeDistance(DEFAULT_IFRAME_DISTANCE),
    minQuality(0),
    maxQuality(0),
    hasDual(false),
    videoOptionsNotSet(true),
    mediaUrl(),
    deviceUrl(),
    reinitDeviceInfo(false)
{
}

bool QnPlOnvifGeneric211Resource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlOnvifGeneric211Resource::updateMACAddress()
{
    return true;
}

QString QnPlOnvifGeneric211Resource::manufacture() const
{
    return MANUFACTURE;
}

QnAbstractStreamDataProvider* QnPlOnvifGeneric211Resource::createLiveDataProvider()
{
    return new QnOnvifGeneric211StreamReader(toSharedPointer());
}

void QnPlOnvifGeneric211Resource::setCropingPhysical(QRect /*croping*/)
{

}

void QnPlOnvifGeneric211Resource::init()
{
    QMutexLocker lock(&m_mutex);

    setOnvifUrls();

    if (!isSoapAuthorized()) {
        reinitDeviceInfo = true;
        setStatus(QnResource::Unauthorized);
        return;
    }

    if (reinitDeviceInfo) {
        fetchAndSetDeviceInformation();
        setOnvifUrls();
        reinitDeviceInfo = false;
    }

    fetchAndSetVideoEncoderOptions();

    save();
}

const ResolutionPair QnPlOnvifGeneric211Resource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolutionList.isEmpty()? EMPTY_RESOLUTION_PAIR: m_resolutionList.front();
}

float QnPlOnvifGeneric211Resource::getResolutionAspectRatio(const ResolutionPair& resolution) const
{
    return resolution.second == 0? 0: static_cast<double>(resolution.first) / resolution.second;
}

const ResolutionPair QnPlOnvifGeneric211Resource::getNearestResolution(const ResolutionPair& resolution, float aspectRatio) const
{
    QMutexLocker lock(&m_mutex);

    int bestIndex = -1;
    double bestMatchCoeff = INT_MAX;
    double requestSquare = resolution.first * resolution.second;
    if (requestSquare == 0) return EMPTY_RESOLUTION_PAIR;

    for (int i = 0; i < m_resolutionList.size(); ++i) {
        float ar = getResolutionAspectRatio(m_resolutionList[i]);
        if (qAbs(ar - aspectRatio) > MAX_AR_EPS) {
            continue;
        }

        double square = m_resolutionList[i].first * m_resolutionList[i].second;
        if (square == 0) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff < bestMatchCoeff) {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }

    return bestIndex >= 0 ? m_resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

int QnPlOnvifGeneric211Resource::getMaxFps()
{
    //Synchronization is not needed
    return maxFps;
}

void QnPlOnvifGeneric211Resource::setMotionMaskPhysical(int channel)
{
    /*QMutexLocker lock(&m_mutex);

    if (m_lastMotionReadTime == 0)
        readMotionInfo();


    const QnMotionRegion region = m_motionMaskList[0];

    QMap<int, QRect> existsWnd = m_motionWindows; // the key is window number
    QMultiMap<int, QRect> newWnd = region.getAllMotionRects(); // the key is motion sensitivity

    while (existsWnd.size() > newWnd.size())
    {
        int key = (existsWnd.end()-1).key();
        removeMotionWindow(key);
        existsWnd.remove(key);
    }
    while (existsWnd.size() < newWnd.size()) {
        int newNum = addMotionWindow();
        existsWnd.insert(newNum, QRect());
    }

    QMap<int, QRect>::iterator cameraWndItr = existsWnd.begin();
    QMap<int, QRect>::iterator motionWndItr = newWnd.begin();
    m_motionWindows.clear();
    while (cameraWndItr != existsWnd.end())
    {
        QRect axisRect = gridRectToAxisRect(motionWndItr.value());
        updateMotionWindow(cameraWndItr.key(), toAxisMotionSensitivity(motionWndItr.key()), axisRect);
        m_motionWindows.insert(cameraWndItr.key(), motionWndItr.value());
        *cameraWndItr = axisRect;
        ++cameraWndItr;
        ++motionWndItr;
    }*/
}

void QnPlOnvifGeneric211Resource::fetchAndSetDeviceInformation()
{
    DeviceBindingProxy soapProxy;
    QString endpoint(deviceUrl.isEmpty()? createOnvifEndpointUrl(): deviceUrl);

    QAuthenticator auth(getAuth());
    std::string login(auth.user().toStdString());
    std::string passwd(auth.password().toStdString());
    if (!login.empty()) soap_register_plugin(soapProxy.soap, soap_wsse);

    //Trying to get name
    _onvifDevice__GetDeviceInformation request;
    _onvifDevice__GetDeviceInformationResponse response;
    if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());

    int soapRes = soapProxy.GetDeviceInformation(endpoint.toStdString().c_str(), NULL, &request, &response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
            << endpoint << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
    } else {
        setName((response.Manufacturer + " - " + response.Model).c_str());
    }

    //Trying to get onvif URLs
    _onvifDevice__GetCapabilities request2;
    _onvifDevice__GetCapabilitiesResponse response2;
    if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());

    soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request2, &response2);
    if (soapRes != SOAP_OK && cl_log.logLevel() >= cl_logDEBUG1) {
        qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
            << endpoint << " failed. GSoap error code: " << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
    }

    if (response2.Capabilities && response2.Capabilities->Media) {
        setMediaUrl(response2.Capabilities->Media->XAddr.c_str());
        setParam(MEDIA_URL_PARAM_NAME, getMediaUrl(), QnDomainDatabase);
    }
    if (response2.Capabilities && response2.Capabilities->Device) {
        setDeviceUrl(response2.Capabilities->Device->XAddr.c_str());
        setParam(DEVICE_URL_PARAM_NAME, getDeviceUrl(), QnDomainDatabase);
    }
}

void QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions()
{
    MediaBindingProxy soapProxy;
    QString endpoint(createOnvifEndpointUrl());

    QAuthenticator auth(getAuth());
    std::string login(auth.user().toStdString());
    std::string passwd(auth.password().toStdString());
    if (!login.empty()) soap_register_plugin(soapProxy.soap, soap_wsse);

    VideoEncoders videoEncoders;

    //Getting video options
    {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetVideoEncoderConfigurationOptions request;
        _onvifMedia__GetVideoEncoderConfigurationOptionsResponse response;

        int soapRes = soapProxy.GetVideoEncoderConfigurationOptions(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK || !response.Options) {
            qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't init ONVIF device resource, will "
                << "try alternative approach (URL: " << endpoint << ", MAC: " << getMAC().toString()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
        } else {
            setVideoEncoderOptions(response);
        }
    }

    //Alternative approach to fetching video encoder options
    if (videoOptionsNotSet) {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetVideoEncoderConfigurations request;

        int soapRes = soapProxy.GetVideoEncoderConfigurations(endpoint.toStdString().c_str(), NULL, &request, &videoEncoders.soapResponse);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't init ONVIF device resource even with alternative approach, "
                << "default settings will be used (URL: " << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: "
                << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
            videoEncoders.soapFailed = true;
        } else {
            analyzeVideoEncoders(videoEncoders, true);
        }
    }

    //Analyzing availability of dual streaming

    if (!videoEncoders.filled && !videoEncoders.soapFailed) {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetVideoEncoderConfigurations request;

        int soapRes = soapProxy.GetVideoEncoderConfigurations(endpoint.toStdString().c_str(), NULL, &request, &videoEncoders.soapResponse);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't feth video encoders info from ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
            videoEncoders.soapFailed = true;
        } else {
            analyzeVideoEncoders(videoEncoders, false);
        }
    }

    int appropriateProfiles = 0;
    {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetProfiles request;
        _onvifMedia__GetProfilesResponse response;

        int soapRes = soapProxy.GetProfiles(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't fetch preset profiles ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
        } else {
            appropriateProfiles = countAppropriateProfiles(response, videoEncoders);
        }
    }

    if (appropriateProfiles >= 2) {
        //Its OK, we have at least two streams
        hasDual = true;
        return;
    }

    if (videoEncoders.soapFailed) {
        //If so, it will be good if we have at least one stream
        //We can't create additional profiles if we no nothing about video encoders
        hasDual = false;
        return;
    }

    int profilesToCreateSize = videoEncoders.videoEncodersUnused.size();
    if (!profilesToCreateSize) {
        //In that case it is impossivle to create new appropriate profiles
        hasDual = false;
        return;
    }

    //Trying to create absent profiles
    std::string profileName = "Netoptix ";
    std::string profileToken = "netoptix";

    profilesToCreateSize = profilesToCreateSize < 2? (profilesToCreateSize - appropriateProfiles): (2 - appropriateProfiles);
    QHash<QString, onvifXsd__VideoEncoderConfiguration*>::const_iterator encodersIter = videoEncoders.videoEncodersUnused.begin();

    for (int i = 0; i < profilesToCreateSize; ++i) {
        if (encodersIter == videoEncoders.videoEncodersUnused.end()) {
            qCritical() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: wrong unused video encoders size!";
            hasDual = false;
            return;
        }

        std::string iStr = QString().setNum(i).toStdString();
        std::string profileNameCurr = profileName + iStr;
        std::string profileTokenCurr = profileToken + iStr;

        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__CreateProfile request;
        request.Name = profileNameCurr;
        request.Token = &profileTokenCurr;
        _onvifMedia__CreateProfileResponse response;

        int soapRes = soapProxy.CreateProfile(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't create new profile for ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: "
                << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());

            hasDual = false;
            return;
        }

        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__AddVideoEncoderConfiguration request2;
        request2.ConfigurationToken = encodersIter.key().toStdString();
        request2.ProfileToken = profileTokenCurr;
        _onvifMedia__AddVideoEncoderConfigurationResponse response2;

        soapRes = soapProxy.AddVideoEncoderConfiguration(endpoint.toStdString().c_str(), NULL, &request2, &response2);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't add video encoder to newly created profile for ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());

            hasDual = false;
            return;
        }

        ++encodersIter;
    }

    hasDual = profilesToCreateSize + appropriateProfiles >= 2;
}

void QnPlOnvifGeneric211Resource::setVideoEncoderOptions(const _onvifMedia__GetVideoEncoderConfigurationOptionsResponse& response) {
    if (!response.Options) {
        return;
    }

    if (response.Options->QualityRange) {
        minQuality = response.Options->QualityRange->Min;
        maxQuality = response.Options->QualityRange->Max;

        qDebug() << "ONVIF quality range [" << minQuality << ", " << maxQuality << "]";
    } else {
        qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch ONVIF quality range";
    }

    if (!response.Options->H264) {
        qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch (ONVIF) max fps, iframe distance and resolution list";
    } else {
        if (response.Options->H264->FrameRateRange) {
            maxFps = response.Options->H264->FrameRateRange->Max;

            qDebug() << "ONVIF max FPS: " << maxFps;
        } else {
            qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch ONVIF max FPS";
        }

        if (response.Options->H264->GovLengthRange) {
            iframeDistance = DEFAULT_IFRAME_DISTANCE <= response.Options->H264->GovLengthRange->Min? response.Options->H264->GovLengthRange->Min:
                (DEFAULT_IFRAME_DISTANCE >= response.Options->H264->GovLengthRange->Max? response.Options->H264->GovLengthRange->Max: DEFAULT_IFRAME_DISTANCE);
            qDebug() << "ONVIF iframe distance: " << iframeDistance;
        } else {
            qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch ONVIF iframe distance";
        }
    }

    std::vector<onvifXsd__VideoResolution*>& resolutions = response.Options->H264->ResolutionsAvailable;
    if (resolutions.size() == 0) {
        qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch ONVIF resolutions";
    } else {
        m_resolutionList.clear();

        std::vector<onvifXsd__VideoResolution*>::const_iterator resPtrIter = resolutions.begin();
        while (resPtrIter != resolutions.end()) {
            m_resolutionList.append(ResolutionPair((*resPtrIter)->Width, (*resPtrIter)->Height));
            ++resPtrIter;
        }
        qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);
    }

    //Printing fetched resolutions
    if (cl_log.logLevel() >= cl_logDEBUG1) {
        qDebug() << "ONVIF resolutions: ";
        foreach (ResolutionPair resolution, m_resolutionList) {
            qDebug() << resolution.first << " x " << resolution.second;
        }
    }

    videoOptionsNotSet = false;
}

void QnPlOnvifGeneric211Resource::analyzeVideoEncoders(VideoEncoders& encoders, bool setOptions) {
    qDebug() << "Alternative video options. Size: " << encoders.soapResponse.Configurations.size();
    std::vector<onvifXsd__VideoEncoderConfiguration*>::const_iterator iter = encoders.soapResponse.Configurations.begin();

    int iframeDistanceMin = INT_MAX;
    int iframeDistanceMax = INT_MIN;

    if (setOptions) {
        minQuality = INT_MAX;
        maxQuality = INT_MIN;
        m_resolutionList.clear();
    }

    encoders.videoEncodersUsed.clear();
    encoders.videoEncodersUnused.clear();
    encoders.filled = true;

    while (iter != encoders.soapResponse.Configurations.end()) {
        onvifXsd__VideoEncoderConfiguration* conf = *iter;

        if (conf->Encoding == onvifXsd__VideoEncoding__H264) {
            QString encodersHashKey(conf->token.c_str());
            if (!encoders.videoEncodersUnused.contains(encodersHashKey)) {
                encoders.videoEncodersUnused.insert(encodersHashKey, conf);
            }

            if (!setOptions) {
                ++iter;
                continue;
            }

            if (minQuality > conf->Quality) minQuality = conf->Quality;
            if (maxQuality < conf->Quality) maxQuality = conf->Quality;

            if (conf->RateControl) {
                if (maxFps < conf->RateControl->FrameRateLimit) maxFps = conf->RateControl->FrameRateLimit;
                if (iframeDistanceMin > conf->RateControl->FrameRateLimit) iframeDistanceMin = conf->RateControl->FrameRateLimit;
                if (iframeDistanceMax < conf->RateControl->FrameRateLimit) iframeDistanceMax = conf->RateControl->FrameRateLimit;
            } else {
                qDebug() << "Alternative video options. Rate Control is absent!!!";
            }

            if (conf->Resolution) {
                m_resolutionList.append(ResolutionPair(conf->Resolution->Width, conf->Resolution->Height));
            } else {
                qDebug() << "Alternative video options. Resolution is absent!!!";
            }
        }

        ++iter;
    }

    if (!setOptions) {
        return;
    }

    if (minQuality > maxQuality) {
        qDebug() << "Alternative video options. Quality was not fetched!";
        minQuality = 0;
        maxQuality = 100;
    }

    if (iframeDistanceMin <= iframeDistanceMax) {
        iframeDistance = DEFAULT_IFRAME_DISTANCE <= iframeDistanceMin? iframeDistanceMin:
            (DEFAULT_IFRAME_DISTANCE >= iframeDistanceMax? iframeDistanceMax: DEFAULT_IFRAME_DISTANCE);
    } else {
        qDebug() << "Alternative video options. Quality was not fetched!";
    }

    qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);

    if (cl_log.logLevel() >= cl_logDEBUG1) {
        qDebug() << "ONVIF quality range [" << minQuality << ", " << maxQuality << "]";
        qDebug() << "ONVIF max FPS: " << maxFps;
        qDebug() << "ONVIF iframe distance: " << iframeDistance;

        qDebug() << "ONVIF resolutions: ";
        foreach (ResolutionPair resolution, m_resolutionList) {
            qDebug() << resolution.first << " x " << resolution.second;
        }
    }

    videoOptionsNotSet = false;
}

int QnPlOnvifGeneric211Resource::innerQualityToOnvif(QnStreamQuality quality) const
{
    if (quality > QnQualityHighest) {
        qWarning() << "QnPlOnvifGeneric211Resource::onvifQualityToInner: got unexpected quality (too big): " << quality;
        return maxQuality;
    }
    if (quality < QnQualityLowest) {
        qWarning() << "QnPlOnvifGeneric211Resource::onvifQualityToInner: got unexpected quality (too small): " << quality;
        return minQuality;
    }

    qDebug() << "QnPlOnvifGeneric211Resource::onvifQualityToInner: in quality = " << quality << ", out qualty = "
             << minQuality + (maxQuality - minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest)
             << ", minOnvifQuality = " << minQuality << ", maxOnvifQuality = " << maxQuality;

    return minQuality + (maxQuality - minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
}

int QnPlOnvifGeneric211Resource::countAppropriateProfiles(const _onvifMedia__GetProfilesResponse& response, VideoEncoders& encoders)
{
    const std::vector<onvifXsd__Profile*>& profiles = response.Profiles;
    int result = 0;

    for (long i = 0; i < profiles.size(); ++i) {
        onvifXsd__Profile* profilePtr = profiles.at(i);
        if (!profilePtr->VideoEncoderConfiguration || profilePtr->VideoEncoderConfiguration->Encoding != onvifXsd__VideoEncoding__H264) {
            continue;
        }

        QString encodersHashKey(profilePtr->VideoEncoderConfiguration->token.c_str());
        onvifXsd__VideoEncoderConfiguration* encoder = encoders.videoEncodersUnused.take(encodersHashKey);
        if (encoder) {
            encoders.videoEncodersUsed.insert(encodersHashKey, encoder);
            ++result;
        }
    }

    return result;
}

bool QnPlOnvifGeneric211Resource::isSoapAuthorized() const {
    DeviceBindingProxy soapProxy;
    QString endpoint(deviceUrl.isEmpty()? createOnvifEndpointUrl(): deviceUrl);

    qDebug() << "QnPlOnvifGeneric211Resource::isSoapAuthorized: deviceUrl is '" << deviceUrl << "'";

    QAuthenticator auth(getAuth());
    std::string login(auth.user().toStdString());
    std::string passwd(auth.password().toStdString());
    if (!login.empty()) {
        soap_register_plugin(soapProxy.soap, soap_wsse);
        soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
    }

    qDebug() << "QnPlOnvifGeneric211Resource::isSoapAuthorized: login = " << login.c_str() << ", password = " << passwd.c_str();

    _onvifDevice__GetCapabilities request;
    _onvifDevice__GetCapabilitiesResponse response;
    int soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request, &response);

    if (soapRes != SOAP_OK && PasswordHelper::isNotAuthenticated(soapProxy.soap_fault())) {
        return false;
    }

    return true;
}

void QnPlOnvifGeneric211Resource::setOnvifUrls()
{
    if (deviceUrl.isEmpty()) {
        QVariant deviceVariant;
        bool res = getParam(DEVICE_URL_PARAM_NAME, deviceVariant, QnDomainDatabase);
        QString deviceStr(res? deviceVariant.toString(): "");

        deviceUrl = deviceStr.isEmpty()? createOnvifEndpointUrl(): deviceStr;

        qDebug() << "QnPlOnvifGeneric211Resource::setOnvifUrls: deviceUrl = " << deviceUrl;
    }

    if (mediaUrl.isEmpty()) {
        QVariant mediaVariant;
        bool res = getParam(MEDIA_URL_PARAM_NAME, mediaVariant, QnDomainDatabase);
        QString mediaStr(res? mediaVariant.toString(): "");

        mediaUrl = mediaStr.isEmpty()? createOnvifEndpointUrl(): mediaStr;

        qDebug() << "QnPlOnvifGeneric211Resource::setOnvifUrls: mediaUrl = " << mediaUrl;
    }
}

void QnPlOnvifGeneric211Resource::save()
{
    QByteArray errorStr;
    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>(), errorStr) != 0) {
        qCritical() << "QnPlOnvifGeneric211Resource::init: can't save resource params to Enterprise Controller. Resource MAC: "
                    << getMAC().toString() << ". Description: " << errorStr;
    }
}
