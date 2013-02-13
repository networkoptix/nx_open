#ifndef sony_resource_h_1855
#define sony_resource_h_1855

#include <map>

#include <QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/linesplitter.h"
#include "core/datapacket/media_data_packet.h"
#include "../onvif/onvif_resource.h"


namespace nx_http
{
    class AsyncHttpClient;
}

class QnPlSonyResource : public QnPlOnvifResource
{
    static int MAX_RESOLUTION_DECREASES_NUM;

    Q_OBJECT

public:
    QnPlSonyResource();
    virtual ~QnPlSonyResource();

    virtual int getMaxOnvifRequestTries() const override { return 5; }

protected:
    virtual bool updateResourceCapabilities() override;
    virtual bool initInternal() override;
    virtual bool startInputPortMonitoring() override;
    virtual void stopInputPortMonitoring() override;
    virtual bool isInputPortMonitored() const override;

private:
    nx_http::AsyncHttpClient* m_inputMonitorHttpClient;
    mutable QMutex m_inputPortMutex;
    nx_http::LineSplitter m_lineSplitter;
    std::map<int, bool> m_relayInputStates;

private slots:
    void onMonitorResponseReceived( nx_http::AsyncHttpClient* httpClient );
    void onMonitorMessageBodyAvailable( nx_http::AsyncHttpClient* httpClient );
    void onMonitorConnectionClosed( nx_http::AsyncHttpClient* httpClient );
};

typedef QnSharedResourcePointer<QnPlSonyResource> QnPlSonyResourcePtr;

#endif //sony_resource_h_1855
