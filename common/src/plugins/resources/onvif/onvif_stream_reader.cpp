#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include <QTextStream>
#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/soapMediaBindingProxy.h"
//#include "onvif/wsseapi.h"
#include "utils/network/tcp_connection_priv.h"


//
// QnOnvifStreamReader
//

QnOnvifStreamReader::QnOnvifStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    m_multiCodec(res)
{
    m_onvifRes = getResource().dynamicCast<QnPlOnvifResource>();
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{

}

void QnOnvifStreamReader::openStream()
{
    if (isStreamOpened())
        return;

    if (!m_onvifRes->isSoapAuthorized()) {
        m_onvifRes->setStatus(QnResource::Unauthorized);
        return;
    }

    QString streamUrl = updateCameraAndfetchStreamUrl();
    if (streamUrl.isEmpty()) {
        qCritical() << "QnOnvifStreamReader::openStream: can't fetch stream URL for resource with MAC: "
                    << m_onvifRes->getMAC().toString();
        return;
    }

    m_multiCodec.setRequest(streamUrl);
    m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED)
        m_resource->setStatus(QnResource::Unauthorized);
}

const QString QnOnvifStreamReader::updateCameraAndfetchStreamUrl() const
{
    QnResource::ConnectionRole role = getRole();

    if (role == QnResource::Role_LiveVideo)
    {
        return updateCameraAndfetchStreamUrl(true);
    }

    if (role == QnResource::Role_SecondaryLiveVideo) {
        return updateCameraAndfetchStreamUrl(false);
    }

    qWarning() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl: "
               << "got unexpected role: " << role;
    return QString();
}

const QString QnOnvifStreamReader::updateCameraAndfetchStreamUrl(bool isPrimary) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString());
    std::string profileToken;

    //Modifying appropriate profile (setting required values)
    {
        //Fetching appropriate profile if it exists
        ProfilesReq request;
        ProfilesResp response;
        int soapRes = soapWrapper.getProfiles(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't set values into ONVIF physical device (URL: " << m_onvifRes->getMediaUrl() << ", UniqueId: "
                << m_onvifRes->getUniqueId() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << ". " << soapWrapper.getLastError();
            return QString();
        }

        onvifXsd__Profile* profilePtr = findAppropriateProfile(response, isPrimary);
        if (!profilePtr) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't set values into ONVIF physical device (URL: "
                << m_onvifRes->getMediaUrl() << ", MAC: " << m_onvifRes->getUniqueId() << "). Root cause: there is no appropriate profile";
            return QString();
        }

        //Setting required values into profile
        profileToken = profilePtr->token;
        SetVideoConfigReq encRequest;
        SetVideoConfigResp encResponse;
        encRequest.Configuration = profilePtr->VideoEncoderConfiguration;

        updateVideoEncoderParams(encRequest.Configuration, isPrimary);

        soapRes = soapWrapper.setVideoEncoderConfiguration(encRequest, encResponse);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't set required values into ONVIF physical device (URL: "
                << m_onvifRes->getMediaUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: SOAP failed. Default settings will be used. GSoap error code: "
                << soapRes << ". " << soapWrapper.getLastError();
        }
    }

    //Printing modified profile
    if (cl_log.logLevel() >= cl_logDEBUG1) {
        ProfileReq request;
        ProfileResp response;
        request.ProfileToken = profileToken;

        if (soapWrapper.getProfile(request, response) == SOAP_OK) {
            printProfile(response, isPrimary);
        }
    }

    //Fetching stream URL
    {
        StreamUriResp response;
        StreamUriReq request;
        onvifXsd__StreamSetup streamSetup;
        onvifXsd__Transport transport;

        request.StreamSetup = &streamSetup;
        request.StreamSetup->Transport = &transport;
        request.StreamSetup->Stream = onvifXsd__StreamType__RTP_Unicast;
        request.StreamSetup->Transport->Tunnel = 0;
        request.StreamSetup->Transport->Protocol = onvifXsd__TransportProtocol__UDP;
        request.ProfileToken = profileToken;

        int soapRes = soapWrapper.getStreamUri(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't get stream URL of ONVIF device (URL: "
                << m_onvifRes->getMediaUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: SOAP request failed. GSoap error code: "
                << soapRes << ". " << soapWrapper.getLastError();
            return QString();
        }

        if (!response.MediaUri) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't get stream URL of ONVIF device (URL: "
                << m_onvifRes->getMediaUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: got empty response.";
            return QString();
        }

        qDebug() << "URL of ONVIF device stream (UniqueId: " << m_onvifRes->getUniqueId()
                 << ") successfully fetched: " << response.MediaUri->Uri.c_str();
        return QString(normalizeStreamSrcUrl(response.MediaUri->Uri));
    }

    return QString();
}

void QnOnvifStreamReader::closeStream()
{
    m_multiCodec.closeStream();
}

bool QnOnvifStreamReader::isStreamOpened() const
{
    return m_multiCodec.isStreamOpened();
}

QnMetaDataV1Ptr QnOnvifStreamReader::getCameraMetadata()
{
    return QnMetaDataV1Ptr(0);
}

bool QnOnvifStreamReader::isGotFrame(QnCompressedVideoDataPtr videoData)
{
    const quint8* curNal = (const quint8*) videoData->data.data();
    const quint8* end = curNal + videoData->data.size();
    curNal = NALUnit::findNextNAL(curNal, end);

    const quint8* nextNal = curNal;
    for (;curNal < end; curNal = nextNal)
    {
        nextNal = NALUnit::findNextNAL(curNal, end);
        quint8 nalUnitType = *curNal & 0x1f;
        if (nalUnitType >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
            return true;
    }
    return false;
}

QnAbstractMediaDataPtr QnOnvifStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_multiCodec.getNextData();
        if (rez) 
        {
            QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
            //ToDo: if (videoData)
            //    parseMotionInfo(videoData);
            
            if (!videoData || isGotFrame(videoData))
                break;
        }
        else {
            errorCount++;
            if (errorCount > 1) {
                closeStream();
                break;
            }
        }
    }
    
    return rez;
}

void QnOnvifStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnOnvifStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

onvifXsd__Profile* QnOnvifStreamReader::findAppropriateProfile(const ProfilesResp& response,
        bool isPrimary) const
{
    const std::vector<onvifXsd__Profile*>& profiles = response.Profiles;
    float maxVal = 0;
    long maxInd = -1;
    float minVal = 0;
    long minInd = -1;

    for (unsigned long i = 0; i < profiles.size(); ++i) {
        onvifXsd__Profile* profilePtr = profiles.at(i);
        if (!profilePtr->VideoEncoderConfiguration/* || 
                profilePtr->VideoEncoderConfiguration->Encoding != onvifXsd__VideoEncoding__H264 && m_onvifRes->getCodec() == QnPlOnvifResource::H264 ||
                profilePtr->VideoEncoderConfiguration->Encoding != onvifXsd__VideoEncoding__JPEG && m_onvifRes->getCodec() == QnPlOnvifResource::JPEG*/) {
            continue;
        }

        float quality = profilePtr->VideoEncoderConfiguration->Quality;
        if (minVal > quality || minVal == quality && maxInd == minInd || minInd == -1) {
            minInd = i;
            minVal = quality;
        }
        if (maxVal < quality || maxVal == quality && maxInd == minInd || maxInd == -1) {
            maxInd = i;
            maxVal = quality;
        }
    }

    if (isPrimary && maxInd != -1) return profiles.at(maxInd);
    if (!isPrimary && minInd != -1 && minInd != maxInd) return profiles.at(minInd);

    return 0;
}

//Deleting protocol, host and port from URL assuming that input URLs cannot be without
//protocol, but with host specified.
const QString QnOnvifStreamReader::normalizeStreamSrcUrl(const std::string& src) const
{
    qDebug() << "QnOnvifStreamReader::normalizeStreamSrcUrl: initial URL: '" << src.c_str() << "'";

    QString result(QString::fromStdString(src));
    int pos = result.indexOf("://");
    if (pos != -1) {
        pos += sizeof("://");
        pos = result.indexOf("/", pos);
        result = pos != -1? result.right(result.size() - pos - 1): QString();
    }

    qDebug() << "QnOnvifStreamReader::normalizeStreamSrcUrl: normalized URL: '" << result << "'";
    return result;
}

void QnOnvifStreamReader::printProfile(const ProfileResp& response, bool isPrimary) const
{
    qDebug() << "ONVIF device (UniqueId: " << m_onvifRes->getUniqueId() << ") has the following "
             << (isPrimary? "primary": "secondary") << " profile:";
    qDebug() << "Name: " << response.Profile->Name.c_str();
    qDebug() << "Token: " << response.Profile->token.c_str();
    qDebug() << "Encoder Name: " << response.Profile->VideoEncoderConfiguration->Name.c_str();
    qDebug() << "Encoder Token: " << response.Profile->VideoEncoderConfiguration->token.c_str();
    qDebug() << "Quality: " << response.Profile->VideoEncoderConfiguration->Quality;
    qDebug() << "Resolution: " << response.Profile->VideoEncoderConfiguration->Resolution->Width << "x"
             << response.Profile->VideoEncoderConfiguration->Resolution->Height;
    qDebug() << "FPS: " << response.Profile->VideoEncoderConfiguration->RateControl->FrameRateLimit;
}

void QnOnvifStreamReader::updateVideoEncoderParams(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const
{
    if (!config) {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: got NULL ptr. UniqueId: " << m_onvifRes->getUniqueId();
        return;
    }

    config->Encoding = m_onvifRes->getCodec() == QnPlOnvifResource::H264? onvifXsd__VideoEncoding__H264: onvifXsd__VideoEncoding__JPEG;
    if (isPrimary) {
        config->Name = "Netoptix Primary";
    } else {    
        config->Name = "Netoptix Secondary";
    }

    if (!config->RateControl) {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: RateControl is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else {
        config->RateControl->FrameRateLimit = getFps();
    }

    QnStreamQuality quality = getQuality();
    if (quality != QnQualityPreSeted) {
        config->Quality = m_onvifRes->innerQualityToOnvif(quality);
    }

    if (!config->Resolution) {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: Resolution is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else {

        ResolutionPair resolution = isPrimary? m_onvifRes->getPrimaryResolution(): m_onvifRes->getSecondaryResolution();
        if (resolution == EMPTY_RESOLUTION_PAIR) {
            qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: : Can't determine (" << (isPrimary? "primary": "secondary") 
                << ") resolution " << "for ONVIF device (UniqueId: " << m_onvifRes->getUniqueId() << "). Default resolution will be used.";
        } else {

            config->Resolution->Width = resolution.first;
            config->Resolution->Height = resolution.second;
        }
    }
}
