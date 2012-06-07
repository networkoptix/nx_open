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
#include "onvif/wsseapi.h"

QnOnvifStreamReader::QnOnvifStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    m_RTP264(res)
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

    m_RTP264.setRequest(streamUrl);
    m_RTP264.openStream();
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
    MediaBindingProxy soapProxy;

    QAuthenticator auth(m_onvifRes->getAuth());
    std::string login(auth.user().toStdString());
    std::string passwd(auth.password().toStdString());
    if (!login.empty()) soap_register_plugin(soapProxy.soap, soap_wsse);

    QString endpoint(m_onvifRes->getMediaUrl());
    std::string profileToken;

    //Modifying appropriate profile (setting required values)
    {
        //Fetching appropriate profile if it exists
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetProfiles request;
        _onvifMedia__GetProfilesResponse response;
        int soapRes = soapProxy.GetProfiles(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't set values into ONVIF physical device (URL: " << endpoint << ", MAC: "
                << m_onvifRes->getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: " << soapRes
                << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
            soap_end(soapProxy.soap);
            return QString();
        }
        soap_end(soapProxy.soap);

        onvifXsd__Profile* profilePtr = findAppropriateProfile(response, isPrimary);
        if (!profilePtr) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't set values into ONVIF physical device (URL: "
                << endpoint << ", MAC: " << m_onvifRes->getMAC().toString() << "). Root cause: there is no appropriate profile";
            return QString();
        }

        profileToken = profilePtr->token;

        //Setting required values into profile
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__SetVideoEncoderConfiguration encRequest;
        _onvifMedia__SetVideoEncoderConfigurationResponse encResponse;
        encRequest.Configuration = profilePtr->VideoEncoderConfiguration;
        setRequiredValues(encRequest.Configuration, isPrimary);

        soapRes = soapProxy.SetVideoEncoderConfiguration(endpoint.toStdString().c_str(), NULL, &encRequest, &encResponse);
        if (soapRes != SOAP_OK) {
            qWarning() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't set values into ONVIF physical device (URL: "
                << endpoint << ", MAC: " << m_onvifRes->getMAC().toString() << "). Root cause: SOAP failed. Default settings will be used. GSoap error code: "
                << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
        }
        soap_end(soapProxy.soap);
    }

    //Printing modified profile
    if (cl_log.logLevel() >= cl_logDEBUG1) {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetProfile request;
        _onvifMedia__GetProfileResponse response;
        request.ProfileToken = profileToken;

        if (soapProxy.GetProfile(endpoint.toStdString().c_str(), NULL, &request, &response) == SOAP_OK) {
            printProfile(response, isPrimary);
        }
        soap_end(soapProxy.soap);
    }

    //Fetching stream URL
    {
        if (!login.empty()) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login.c_str(), passwd.c_str());
        _onvifMedia__GetStreamUriResponse response;
        _onvifMedia__GetStreamUri request;
        onvifXsd__StreamSetup streamSetup;
        onvifXsd__Transport transport;

        request.StreamSetup = &streamSetup;
        request.StreamSetup->Transport = &transport;
        request.StreamSetup->Stream = onvifXsd__StreamType__RTP_Unicast;
        request.StreamSetup->Transport->Tunnel = 0;
        request.StreamSetup->Transport->Protocol = onvifXsd__TransportProtocol__UDP;
        request.ProfileToken = profileToken;

        int soapRes = soapProxy.GetStreamUri(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't get stream URL of ONVIF device (URL: "
                << endpoint << ", MAC: " << m_onvifRes->getMAC().toString() << "). Root cause: SOAP request failed. GSoap error code: "
                << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
            soap_end(soapProxy.soap);
            return QString();
        }
        soap_end(soapProxy.soap);

        if (!response.MediaUri) {
            qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl (primary stream = "
                << isPrimary << "): can't get stream URL of ONVIF device (URL: "
                << endpoint << ", MAC: " << m_onvifRes->getMAC().toString() << "). Root cause: got empty response.";
            return QString();
        }

        qDebug() << "URL of ONVIF device stream (MAC: " << m_onvifRes->getMAC().toString()
                 << ") successfully fetched: " << response.MediaUri->Uri.c_str();
        return QString(normalizeStreamSrcUrl(response.MediaUri->Uri));
    }

    return QString();
}

void QnOnvifStreamReader::closeStream()
{
    m_RTP264.closeStream();
}

bool QnOnvifStreamReader::isStreamOpened() const
{
    return m_RTP264.isStreamOpened();
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

    //if (needMetaData())
    //    return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_RTP264.getNextData();
        if (rez) 
        {
            QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
            //ToDo: if (videoData)
            //    parseMotionInfo(videoData);
            
            if (isGotFrame(videoData))
                break;
        }
        else {
            closeStream();
            break;
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

onvifXsd__Profile* QnOnvifStreamReader::findAppropriateProfile(const _onvifMedia__GetProfilesResponse& response,
        bool isPrimary) const
{
    const std::vector<onvifXsd__Profile*>& profiles = response.Profiles;
    float maxVal = 0;
    long maxInd = -1;
    float minVal = 0;
    long minInd = -1;

    for (long i = 0; i < profiles.size(); ++i) {
        onvifXsd__Profile* profilePtr = profiles.at(i);
        if (!profilePtr->VideoEncoderConfiguration || profilePtr->VideoEncoderConfiguration->Encoding != onvifXsd__VideoEncoding__H264) {
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

void QnOnvifStreamReader::setRequiredValues(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const
{
    if (!config) {
        qWarning() << "QnOnvifStreamReader::setRequiredValues: got NULL ptr";
        return;
    }

    config->Name = isPrimary? "Netoptix Primary Encoder": "Netoptix Secondary Encoder";
    config->RateControl->FrameRateLimit = getFps();

    QnStreamQuality quality = getQuality();
    if (quality != QnQualityPreSeted)
        config->Quality = m_onvifRes->innerQualityToOnvif(quality);

    ResolutionPair maxResolution = m_onvifRes->getMaxResolution();
    if (maxResolution == EMPTY_RESOLUTION_PAIR) {
        qWarning() << "QnOnvifStreamReader::setRequiredValues: Can't determine max resolution for ONVIF device (MAC: "
                   << m_onvifRes->getMAC().toString() << "). Default resolution will be used.";
    } else if (isPrimary) {
        config->Resolution->Width = maxResolution.first;
        config->Resolution->Height = maxResolution.second;
    } else {
        ResolutionPair resolution = m_onvifRes->getNearestResolution(
            SECONDARY_STREAM_DEFAULT_RESOLUTION, m_onvifRes->getResolutionAspectRatio(maxResolution));

        if (resolution != EMPTY_RESOLUTION_PAIR) {
            config->Resolution->Width = resolution.first;
            config->Resolution->Height = resolution.second;
        } else {
            qWarning() << "QnOnvifStreamReader::setRequiredValues: "
                       << "Can't determine resolution for secondary stream of ONVIF device (MAC: "
                       << m_onvifRes->getMAC().toString() << "). Default resolution will be used.";
        }
    }
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

void QnOnvifStreamReader::printProfile(const _onvifMedia__GetProfileResponse& response, bool isPrimary) const
{
    qDebug() << "ONVIF device (MAC: " << m_onvifRes->getMAC().toString() << ") has the following "
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
