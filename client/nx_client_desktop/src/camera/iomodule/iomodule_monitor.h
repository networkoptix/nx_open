#pragma once

#include <client_core/connection_context_aware.h>

#include "api/model/api_ioport_data.h"
#include "core/resource/resource_fwd.h"

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/thread/mutex.h>

class QnIOModuleMonitor: public QObject, public QnConnectionContextAware
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
