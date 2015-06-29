#include "iomodule_monitor.h"
#include "utils/common/log.h"
#include "core/resource/camera_resource.h"

QnIOModuleMonitor::QnIOModuleMonitor(const QnSecurityCamResourcePtr camera):
        m_camera(camera)
{
    reopen();
}

void QnIOModuleMonitor::reopen()
{
    QMutexLocker lk( &m_mutex );

    m_connection = std::make_shared<nx_http::AsyncHttpClient>();
    connect( m_connection.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnIOModuleMonitor::at_MonitorResponseReceived, Qt::DirectConnection );
    connect( m_connection.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable, this, &QnIOModuleMonitor::at_MonitorMessageBodyAvailable, Qt::DirectConnection );
    connect( m_connection.get(), &nx_http::AsyncHttpClient::done, this, &QnIOModuleMonitor::at_MonitorConnectionClosed, Qt::DirectConnection );

    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
}

void QnIOModuleMonitor::at_MonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
{
    Q_ASSERT( httpClient );

    if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( lit("Axis camera %1. Failed to subscribe to input port(s) monitoring. %2").
            arg(m_camera->getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );

        QMutexLocker lk( &m_mutex );
        if( m_connection == httpClient ) {
            m_connection.reset();
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
        if( m_connection == httpClient ) {
            m_connection.reset();
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
