
#ifdef ENABLE_ONVIF

#include "flexwatch_resource.h"
#include "onvif/soapDeviceBindingProxy.h"

QnFlexWatchResource::QnFlexWatchResource():
    QnPlOnvifResource()
{
    // TODO: #Elric set vendor here
    m_tmpH264Conf = new onvifXsd__H264Configuration;
}

QnFlexWatchResource::~QnFlexWatchResource()
{
    delete m_tmpH264Conf;
}

CameraDiagnostics::Result QnFlexWatchResource::initializeCameraDriver()
{
    CameraDiagnostics::Result rez = QnPlOnvifResource::initializeCameraDriver();
    if (rez.errorCode == CameraDiagnostics::ErrorCode::noError && getChannel() == 0)
        rez = fetchUpdateVideoEncoder();
    return rez;
}

CameraDiagnostics::Result QnFlexWatchResource::fetchUpdateVideoEncoder()
{
    QAuthenticator auth = getAuth();
    MediaSoapWrapper soapWrapper(getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), getTimeDrift());

    VideoConfigsReq request;
    VideoConfigsResp response;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoEncoder: can't get video encoders from camera (" 
            << ") Gsoap error: " << soapRes << ". Description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << getUniqueId();
        return CameraDiagnostics::UnknownErrorResult();
    }

    bool needReboot = false;
    for (uint i = 0; i < response.Configurations.size(); ++i)
    {
        if (response.Configurations[i]->Encoding != onvifXsd__VideoEncoding__H264)
        {
            needReboot = true;
 
            response.Configurations[i]->Encoding = onvifXsd__VideoEncoding__H264;
            VideoEncoder* encoder = response.Configurations[i];
            if (encoder->H264 == 0)
                encoder->H264 = m_tmpH264Conf;

            encoder->H264->GovLength = 10;
            int profile = getPrimaryH264Profile();
            if (profile != -1)
                encoder->H264->H264Profile = onvifXsd__H264Profile(profile);
            if (encoder->RateControl)
                encoder->RateControl->EncodingInterval = 1;
            sendVideoEncoderToCamera(*encoder);
        }
    }
    if (needReboot)
        rebootDevice();
    //return !needReboot;
    return needReboot
        ? CameraDiagnostics::Result( CameraDiagnostics::ErrorCode::unknown )
        : CameraDiagnostics::Result( CameraDiagnostics::ErrorCode::noError );
}

bool QnFlexWatchResource::rebootDevice()
{
    QUrl url(getMediaUrl());
    CLSimpleHTTPClient httpClient(url.host(), url.port(80), 1000*3, getAuth());
    return httpClient.doGET(QLatin1String("cgi-bin/admin/fwdosyscmd.cgi?Command=/sbin/reboot&FwCgiVer=0x0001&RetPage=/admin/close_all.asp")) == CL_HTTP_SUCCESS;
}

#endif //ENABLE_ONVIF
