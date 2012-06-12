#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include <climits>
#include <QHash>
#include <cmath>
#include "onvif_resource.h"
//#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "onvif_stream_reader.h"
#include "onvif_helper.h"
#include "utils/common/synctime.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapMediaBindingProxy.h"
#include "onvif/wsseapi.h"
#include "api/app_server_connection.h"

const char* QnPlOnvifResource::MANUFACTURE = "OnvifDevice";
static const float MAX_EPS = 0.01;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifResource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifResource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifResource::DEFAULT_IFRAME_DISTANCE = 20;
const QString& QnPlOnvifResource::MEDIA_URL_PARAM_NAME = *(new QString("MediaUrl"));
const QString& QnPlOnvifResource::DEVICE_URL_PARAM_NAME = *(new QString("DeviceUrl"));
const float QnPlOnvifResource::QUALITY_COEF = 0.2;

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
// QnPlOnvifResource
//

const QString QnPlOnvifResource::createOnvifEndpointUrl(const QString& ipAddress) {
    return ONVIF_PROTOCOL_PREFIX + ipAddress + ONVIF_URL_SUFFIX;
}

QnPlOnvifResource::QnPlOnvifResource() :
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

QnAbstractStreamDataProvider* QnPlOnvifResource::createLiveDataProvider()
{
    return new QnOnvifStreamReader(toSharedPointer());
}

void QnPlOnvifResource::setCropingPhysical(QRect /*croping*/)
{

}

void QnPlOnvifResource::init()
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

const ResolutionPair QnPlOnvifResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolutionList.isEmpty()? EMPTY_RESOLUTION_PAIR: m_resolutionList.front();
}

float QnPlOnvifResource::getResolutionAspectRatio(const ResolutionPair& resolution) const
{
    return resolution.second == 0? 0: static_cast<double>(resolution.first) / resolution.second;
}

const ResolutionPair QnPlOnvifResource::getNearestResolutionForSecondary(const ResolutionPair& resolution, float aspectRatio) const
{
    return getNearestResolution(resolution, aspectRatio, MAX_SECONDARY_RESOLUTION_SQUARE);
}

const ResolutionPair QnPlOnvifResource::getNearestResolution(const ResolutionPair& resolution, float aspectRatio,
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

int QnPlOnvifResource::getMaxFps()
{
    //Synchronization is not needed
    return maxFps;
}

void QnPlOnvifResource::setMotionMaskPhysical(int channel)
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

void QnPlOnvifResource::fetchAndSetDeviceInformation()
{
    DeviceBindingProxy soapProxy;
    QString endpoint(deviceUrl);

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
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: GetDeviceInformation SOAP to endpoint "
            << endpoint << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
    } else {
        setName((response.Manufacturer + " - " + response.Model).c_str());
    }
    soap_end(soapProxy.soap);

    //Trying to get onvif URLs
    _onvifDevice__GetCapabilities request2;
    _onvifDevice__GetCapabilitiesResponse response2;
    if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());

    soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request2, &response2);
    if (soapRes != SOAP_OK && cl_log.logLevel() >= cl_logDEBUG1) {
        qWarning() << "QnPlOnvifResource::fetchAndSetDeviceInformation: can't fetch media and device URLs. Reason: SOAP to endpoint "
            << endpoint << " failed. GSoap error code: " << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
    }
    soap_end(soapProxy.soap);

    if (response2.Capabilities && response2.Capabilities->Media) {
        setMediaUrl(response2.Capabilities->Media->XAddr.c_str());
        setParam(MEDIA_URL_PARAM_NAME, getMediaUrl(), QnDomainDatabase);
    }
    if (response2.Capabilities && response2.Capabilities->Device) {
        setDeviceUrl(response2.Capabilities->Device->XAddr.c_str());
        setParam(DEVICE_URL_PARAM_NAME, getDeviceUrl(), QnDomainDatabase);
    }
}

void QnPlOnvifResource::fetchAndSetVideoEncoderOptions()
{
    MediaBindingProxy soapProxy;
    QString endpoint(mediaUrl);

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
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't init ONVIF device resource, will "
                << "try alternative approach (URL: " << endpoint << ", MAC: " << getMAC().toString()
                << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
        } else {
            setVideoEncoderOptions(response);
        }
        soap_end(soapProxy.soap);
    }

    //Alternative approach to fetching video encoder options
    if (videoOptionsNotSet) {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetVideoEncoderConfigurations request;

        int soapRes = soapProxy.GetVideoEncoderConfigurations(endpoint.toStdString().c_str(), NULL, &request, &videoEncoders.soapResponse);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't init ONVIF device resource even with alternative approach, "
                << "default settings will be used (URL: " << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: "
                << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
            videoEncoders.soapFailed = true;
        } else {
            analyzeVideoEncoders(videoEncoders, true);
        }
        soap_end(soapProxy.soap);
    }

    //Analyzing availability of dual streaming

    hasDual = getNearestResolutionForSecondary(SECONDARY_STREAM_DEFAULT_RESOLUTION, getResolutionAspectRatio(getMaxResolution())) != EMPTY_RESOLUTION_PAIR;
    //If we have no appropriate resolution - dual streaming is not possible
    if (!hasDual) {
        return;
    }

    if (!videoEncoders.filled && !videoEncoders.soapFailed) {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetVideoEncoderConfigurations request;

        int soapRes = soapProxy.GetVideoEncoderConfigurations(endpoint.toStdString().c_str(), NULL, &request, &videoEncoders.soapResponse);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't feth video encoders info from ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
            videoEncoders.soapFailed = true;
        } else {
            analyzeVideoEncoders(videoEncoders, false);
        }
        soap_end(soapProxy.soap);
    }

    int appropriateProfiles = 0;
    {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetProfiles request;
        _onvifMedia__GetProfilesResponse response;

        int soapRes = soapProxy.GetProfiles(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't fetch preset profiles ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
        } else {
            appropriateProfiles = countAppropriateProfiles(response, videoEncoders);
        }
        soap_end(soapProxy.soap);
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
            qCritical() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: wrong unused video encoders size!";
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
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't create new profile for ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: "
                << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());

            hasDual = false;
            soap_end(soapProxy.soap);
            return;
        }
        soap_end(soapProxy.soap);

        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__AddVideoEncoderConfiguration request2;
        request2.ConfigurationToken = encodersIter.key().toStdString();
        request2.ProfileToken = profileTokenCurr;
        _onvifMedia__AddVideoEncoderConfigurationResponse response2;

        soapRes = soapProxy.AddVideoEncoderConfiguration(endpoint.toStdString().c_str(), NULL, &request2, &response2);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnPlOnvifResource::fetchAndSetVideoEncoderOptions: can't add video encoder to newly created profile for ONVIF device (URL: "
                << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());

            hasDual = false;
            soap_end(soapProxy.soap);
            return;
        }
        soap_end(soapProxy.soap);

        ++encodersIter;
    }

    hasDual = profilesToCreateSize + appropriateProfiles >= 2;
}

void QnPlOnvifResource::setVideoEncoderOptions(const _onvifMedia__GetVideoEncoderConfigurationOptionsResponse& response) {
    if (!response.Options) {
        return;
    }

    if (response.Options->QualityRange) {
		setMinMaxQuality(response.Options->QualityRange->Min, response.Options->QualityRange->Max);

        qDebug() << "ONVIF quality range [" << minQuality << ", " << maxQuality << "]";
    } else {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: can't fetch ONVIF quality range";
    }

    if (!response.Options->H264) {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: can't fetch (ONVIF) max fps, iframe distance and resolution list";
    } else {
        if (response.Options->H264->FrameRateRange) {
            maxFps = response.Options->H264->FrameRateRange->Max;

            qDebug() << "ONVIF max FPS: " << maxFps;
        } else {
            qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: can't fetch ONVIF max FPS";
        }

        if (response.Options->H264->GovLengthRange) {
            iframeDistance = DEFAULT_IFRAME_DISTANCE <= response.Options->H264->GovLengthRange->Min? response.Options->H264->GovLengthRange->Min:
                (DEFAULT_IFRAME_DISTANCE >= response.Options->H264->GovLengthRange->Max? response.Options->H264->GovLengthRange->Max: DEFAULT_IFRAME_DISTANCE);
            qDebug() << "ONVIF iframe distance: " << iframeDistance;
        } else {
            qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: can't fetch ONVIF iframe distance";
        }
    }

    std::vector<onvifXsd__VideoResolution*>& resolutions = response.Options->H264->ResolutionsAvailable;
    if (resolutions.size() == 0) {
        qCritical() << "QnPlOnvifResource::setVideoEncoderOptions: can't fetch ONVIF resolutions";
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

void QnPlOnvifResource::analyzeVideoEncoders(VideoEncoders& encoders, bool setOptions) {
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

int QnPlOnvifResource::innerQualityToOnvif(QnStreamQuality quality) const
{
    if (quality > QnQualityHighest) {
        qWarning() << "QnPlOnvifResource::onvifQualityToInner: got unexpected quality (too big): " << quality;
        return maxQuality;
    }
    if (quality < QnQualityLowest) {
        qWarning() << "QnPlOnvifResource::onvifQualityToInner: got unexpected quality (too small): " << quality;
        return minQuality;
    }

    qDebug() << "QnPlOnvifResource::onvifQualityToInner: in quality = " << quality << ", out qualty = "
             << minQuality + (maxQuality - minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest)
             << ", minOnvifQuality = " << minQuality << ", maxOnvifQuality = " << maxQuality;

    return minQuality + (maxQuality - minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
}

int QnPlOnvifResource::countAppropriateProfiles(const _onvifMedia__GetProfilesResponse& response, VideoEncoders& encoders)
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

bool QnPlOnvifResource::isSoapAuthorized() const {
    DeviceBindingProxy soapProxy;
    QString endpoint(deviceUrl);

    qDebug() << "QnPlOnvifResource::isSoapAuthorized: deviceUrl is '" << deviceUrl << "'";

    QAuthenticator auth(getAuth());
    std::string login(auth.user().toStdString());
    std::string passwd(auth.password().toStdString());
    if (!login.empty()) {
        soap_register_plugin(soapProxy.soap, soap_wsse);
        soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
    }

    qDebug() << "QnPlOnvifResource::isSoapAuthorized: login = " << login.c_str() << ", password = " << passwd.c_str();

    _onvifDevice__GetCapabilities request;
    _onvifDevice__GetCapabilitiesResponse response;
    int soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request, &response);

    if (soapRes != SOAP_OK && PasswordHelper::isNotAuthenticated(soapProxy.soap_fault())) {
        soap_end(soapProxy.soap);
        return false;
    }

    soap_end(soapProxy.soap);
    return true;
}

void QnPlOnvifResource::setOnvifUrls()
{
    if (deviceUrl.isEmpty()) {
        QVariant deviceVariant;
        bool res = getParam(DEVICE_URL_PARAM_NAME, deviceVariant, QnDomainDatabase);
        QString deviceStr(res? deviceVariant.toString(): "");

        deviceUrl = deviceStr.isEmpty()? createOnvifEndpointUrl(): deviceStr;

        qDebug() << "QnPlOnvifResource::setOnvifUrls: deviceUrl = " << deviceUrl;
    }

    if (mediaUrl.isEmpty()) {
        QVariant mediaVariant;
        bool res = getParam(MEDIA_URL_PARAM_NAME, mediaVariant, QnDomainDatabase);
        QString mediaStr(res? mediaVariant.toString(): "");

        mediaUrl = mediaStr.isEmpty()? createOnvifEndpointUrl(): mediaStr;

        qDebug() << "QnPlOnvifResource::setOnvifUrls: mediaUrl = " << mediaUrl;
    }
}

void QnPlOnvifResource::save()
{
    QByteArray errorStr;
    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>(), errorStr) != 0) {
        qCritical() << "QnPlOnvifResource::init: can't save resource params to Enterprise Controller. Resource MAC: "
                    << getMAC().toString() << ". Description: " << errorStr;
    }
}

void QnPlOnvifResource::setMinMaxQuality(int min, int max)
{
	//return minQuality + (maxQuality - minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
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

	minQuality = min + shift;
	maxQuality = max - shift;
}

int QnPlOnvifResource::round(float value)
{
	float floorVal = floorf(value);
    return floorVal - value < 0.5? (int)value: (int)value + 1;
}
