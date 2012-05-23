#include "onvif_211_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "onvif_211_stream_reader.h"
#include "utils/common/synctime.h"
//#include "Onvif.nsmap"
#include "soapMediaBindingProxy.h"
#include "/usr/include/openssl/evp.h"
#include "wsseapi.h"

const char* QnPlOnvifGeneric211Resource::MANUFACTURE = "OnvifDevice";
static const float MAX_AR_EPS = 0.01;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
const char* QnPlOnvifGeneric211Resource::ONVIF_PROTOCOL_PREFIX = "http://";
const char* QnPlOnvifGeneric211Resource::ONVIF_URL_SUFFIX = ":80/onvif/device_service";
const int QnPlOnvifGeneric211Resource::DEFAULT_IFRAME_DISTANCE = 20;


//width > height is prefered
bool resolutionGreaterThan(const ResolutionPair &s1, const ResolutionPair &s2)
{
    long long res1 = s1.first * s1.second;
    long long res2 = s2.first * s2.second;
    return res1 > res2? true: (res1 == res2 && s1.first > s2.first? true: false);
}

const QString QnPlOnvifGeneric211Resource::createOnvifEndpointUrl(const QString ipAddress) {
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
    maxQuality(0)
{
    //ToDo: setAuth("root", "1");
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
    fetchAndSetVideoEncoderOptions();
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

void QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions() {
    MediaBindingProxy soapProxy;
    soap_register_plugin(soapProxy.soap, soap_wsse);
    //soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", "root", "admin123");
    //soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", "admin", "admin");
    QString endpoint(createOnvifEndpointUrl());
    qDebug() << "Endpoint: " << endpoint;
    _onvifMedia__GetVideoEncoderConfigurationOptions request;
    _onvifMedia__GetVideoEncoderConfigurationOptionsResponse response;

    int soapRes = soapProxy.GetVideoEncoderConfigurationOptions(endpoint.toStdString().c_str(), NULL, &request, &response);
    if (soapRes != SOAP_OK || !response.Options) {
        qCritical() << "QnPlOnvifGeneric211Resource::fetchAndSetVideoEncoderOptions: can't init ONVIF device resource (URL: "
            << endpoint << ", MAC: " << getMAC().toString() << "). Root cause: SOAP request failed. Error code: " << soapRes
            << "Description: " << soapProxy.soap_fault_string() << ". " << soapProxy.soap_fault_detail();
        return;
    }

    setVideoEncoderOptions(response);
}

void QnPlOnvifGeneric211Resource::setVideoEncoderOptions(const _onvifMedia__GetVideoEncoderConfigurationOptionsResponse& response) {
    if (response.Options->QualityRange) {
        minQuality = response.Options->QualityRange->Min;
        maxQuality = response.Options->QualityRange->Max;

        qDebug() << "ONVIF quality range [" << minQuality << ", " << maxQuality << "]";
    } else {
        qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch ONVIF quality range";
    }

    if (!response.Options->H264) {
        qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch (ONVIF) max fps, iframe distance and resolution list";
        return;
    }

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

    std::vector<onvifXsd__VideoResolution*>& resolutions = response.Options->H264->ResolutionsAvailable;
    if (resolutions.size() == 0) {
        qCritical() << "QnPlOnvifGeneric211Resource::setVideoEncoderOptions: can't fetch ONVIF resolutions";
        return;
    }
    if (!m_resolutionList.isEmpty()) {
        m_resolutionList.clear();
    }

    std::vector<onvifXsd__VideoResolution*>::const_iterator resPtrIter = resolutions.begin();
    while (resPtrIter != resolutions.end()) {
        m_resolutionList.append(ResolutionPair((*resPtrIter)->Width, (*resPtrIter)->Height));
        ++resPtrIter;
    }
    qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreaterThan);

    //Printing fetched resolutions
    if (cl_log.logLevel() >= cl_logDEBUG1) {
        qDebug() << "ONVIF resolutions: ";
        foreach (ResolutionPair resolution, m_resolutionList) {
            qDebug() << resolution.first << " x " << resolution.second;
        }
    }
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
    return minQuality + (maxQuality - minQuality) * (quality - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
}
