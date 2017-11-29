#ifndef sony_resource_h_1855
#define sony_resource_h_1855

#ifdef ENABLE_ONVIF

#include <map>
#include <memory>

#include <nx/utils/thread/mutex.h>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/line_splitter.h>
#include <nx/network/http/asynchttpclient.h>
#include "nx/streaming/media_data_packet.h"
#include "../onvif/onvif_resource.h"


class QnPlSonyResource : public QnPlOnvifResource
{
    static int MAX_RESOLUTION_DECREASES_NUM;

    Q_OBJECT

public:
    QnPlSonyResource();
    virtual ~QnPlSonyResource();

    virtual int getMaxOnvifRequestTries() const override { return 5; }

protected:
    virtual CameraDiagnostics::Result customInitialization(const CapabilitiesResp& capabilitiesResponse) override;

    virtual CameraDiagnostics::Result updateResourceCapabilities() override;
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    virtual void stopInputPortMonitoringAsync() override;
    virtual bool isInputPortMonitored() const override;

private:
    nx_http::AsyncHttpClientPtr m_inputMonitorHttpClient;
    mutable QnMutex m_inputPortMutex;
    nx_http::LineSplitter m_lineSplitter;
    std::map<int, bool> m_relayInputStates;

private slots:
    void onMonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient );
    void onMonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient );
    void onMonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient );
};

//typedef QnSharedResourcePointer<QnPlSonyResource> QnPlSonyResourcePtr;

#endif //ENABLE_ONVIF

#endif //sony_resource_h_1855
