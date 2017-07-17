#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/network/http/httpclient.h>

class QnHikvisionOnvifResource: public QnPlOnvifResource
{
    Q_OBJECT
public:
    QnHikvisionOnvifResource();
    virtual ~QnHikvisionOnvifResource() override;
protected:
    virtual CameraDiagnostics::Result initInternal() override;
private:
    CameraDiagnostics::Result initialize2WayAudio();
    std::unique_ptr<nx_http::HttpClient> getHttpClient();
};

typedef QnSharedResourcePointer<QnHikvisionOnvifResource> QnHikvisionOnvifResourcePtr;

#endif  //ENABLE_ONVIF
