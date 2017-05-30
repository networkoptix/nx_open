#ifndef __IOMODULE_MONITOR_H_
#define __IOMODULE_MONITOR_H_

#include <nx/utils/thread/mutex.h>

#include "api/model/api_ioport_data.h"
#include "core/resource/resource_fwd.h"
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>

class AbstractByteStreamFilter;

class QnIOModuleMonitor: public QObject
{
    Q_OBJECT
public:
    QnIOModuleMonitor(const QnSecurityCamResourcePtr &camera);
    virtual ~QnIOModuleMonitor() override;
    bool open();
signals:
    void connectionClosed();
    void connectionOpened();
    void ioStateChanged(const QnIOStateData& value);
private slots:
    void at_MonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient );
    void at_MonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient );
    void at_MonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient );
private:
    mutable QnMutex m_mutex;
    QnSecurityCamResourcePtr m_camera;
    nx_http::AsyncHttpClientPtr m_httpClient;
    std::shared_ptr<nx_http::MultipartContentParser> m_multipartContentParser;
};

typedef QSharedPointer<QnIOModuleMonitor> QnIOModuleMonitorPtr;

#endif // __IOMODULE_MONITOR_H_
