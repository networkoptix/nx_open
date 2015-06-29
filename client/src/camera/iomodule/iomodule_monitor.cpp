#include "iomodule_monitor.h"
#include "utils/common/log.h"
#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "utils/serialization/json.h"
#include <utils/common/model_functions.h>
#include "http/custom_headers.h"
#include "utils/network/router.h"

class QnMessageBodyParser: public AbstractByteStreamFilter
{
public:
    QnMessageBodyParser(QnIOModuleMonitor* owner): m_owner(owner) {}
protected:
    virtual bool processData( const QnByteArrayConstRef& data ) override
    {
        bool ok = false;
        QnIOStateData value = QJson::deserialized<QnIOStateData>(data, QnIOStateData(), &ok);
        if (ok)
            emit m_owner->ioStateChanged(value);
        else
            qWarning() << Q_FUNC_INFO << "Invalid data to deserialize IO state:" << data;
        return true;
    }
private:
    QnIOModuleMonitor* m_owner;
};

QnIOModuleMonitor::QnIOModuleMonitor(const QnSecurityCamResourcePtr camera):
        m_camera(camera)
{
    reopen();
}

bool QnIOModuleMonitor::reopen()
{
    QMutexLocker lk( &m_mutex );

    m_httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    connect( m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnIOModuleMonitor::at_MonitorResponseReceived, Qt::DirectConnection );
    connect( m_httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable, this, &QnIOModuleMonitor::at_MonitorMessageBodyAvailable, Qt::DirectConnection );
    connect( m_httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnIOModuleMonitor::at_MonitorConnectionClosed, Qt::DirectConnection );

    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
    m_multipartContentParser->setNextFilter(std::make_shared<QnMessageBodyParser>(this));

    QnMediaServerResourcePtr server = m_camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return false;

    m_httpClient->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    QUrl requestUrl(server->getUrl());

    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (!route.gatewayId.isNull()) {
        Q_ASSERT(!route.addr.isNull());
        requestUrl.setHost(route.addr.address.toString());
        requestUrl.setPort(route.addr.port);
    }

    return m_httpClient->doGet( requestUrl );
}

void QnIOModuleMonitor::at_MonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
{
    Q_ASSERT( httpClient );

    if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( lit("Axis camera %1. Failed to subscribe to input port(s) monitoring. %2").
            arg(m_camera->getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );

        QMutexLocker lk( &m_mutex );
        if( m_httpClient == httpClient ) {
            m_httpClient.reset();
            reopen();
        }
        return;
    }

    //analyzing response headers (if needed)
    if( !m_multipartContentParser->setContentType(httpClient->contentType()) )
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        //unexpected content type
        NX_LOG( lit("Error monitoring IO port(s) on Axis camera %1. Unexpected Content-Type (%2) in monitor response. Expected: %3").
            arg(m_camera->getUrl()).arg(QLatin1String(httpClient->contentType())).arg(QLatin1String(multipartContentType)), cl_logWARNING );

        QMutexLocker lk( &m_mutex );
        if( m_httpClient == httpClient ) {
            m_httpClient.reset();
            reopen();
        }
        return;
    }
}

void QnIOModuleMonitor::at_MonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
{
}

void QnIOModuleMonitor::at_MonitorConnectionClosed( nx_http::AsyncHttpClientPtr /*httpClient*/ )
{
    reopen(); // reopen connection
}
