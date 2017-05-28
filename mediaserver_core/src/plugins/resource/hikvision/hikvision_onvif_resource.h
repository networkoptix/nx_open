#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/network/http/http_client.h>

class QnHikvisionOnvifResource: public QnPlOnvifResource
{
    Q_OBJECT
public:
    QnHikvisionOnvifResource();
    virtual ~QnHikvisionOnvifResource() override;
    virtual QnAudioTransmitterPtr getAudioTransmitter() override;
protected:
    virtual CameraDiagnostics::Result initInternal() override;
private:
    CameraDiagnostics::Result initialize2WayAudio();
    std::unique_ptr<nx_http::HttpClient> getHttpClient();
private:
    QnAudioTransmitterPtr m_audioTransmitter;
};

typedef QnSharedResourcePointer<QnHikvisionOnvifResource> QnHikvisionOnvifResourcePtr;

#endif  //ENABLE_ONVIF
