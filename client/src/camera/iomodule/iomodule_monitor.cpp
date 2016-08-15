#include "iomodule_monitor.h"
#include "utils/common/log.h"
#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "utils/serialization/json.h"
#include <utils/common/model_functions.h>
#include "http/custom_headers.h"
#include "network/router.h"
#include "api/app_server_connection.h"

namespace {
    int HTTP_READ_TIMEOUT = 1000 * 10;
}

class QnMessageBodyParser: public AbstractByteStreamFilter
{
public:
    QnMessageBodyParser(QnIOModuleMonitor* owner): m_owner(owner) {}
protected:
    virtual bool processData( const QnByteArrayConstRef& data ) override
    {
        bool ok = false;
        QnIOStateDataList valueList = QJson::deserialized<QnIOStateDataList>(data, QnIOStateDataList(), &ok);
        if (ok) {
            for (const auto& value: valueList)
                emit m_owner->ioStateChanged(value);
        }
        else
            qWarning() << Q_FUNC_INFO << "Invalid data to deserialize IO state:" << data;
        return true;
    }
private:
    QnIOModuleMonitor* m_owner;
};

QnIOModuleMonitor::QnIOModuleMonitor(const QnSecurityCamResourcePtr &camera):
    m_camera(camera)
{
}

bool QnIOModuleMonitor::open()
{
    {
        nx_http::AsyncHttpClientPtr httpClient;
        QnMutexLocker lk(&m_mutex);
        std::swap(httpClient, m_httpClient);
    }

    auto m_httpClient = nx_http::AsyncHttpClient::create();
    m_httpClient->setMessageBodyReadTimeoutMs(HTTP_READ_TIMEOUT);

    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &QnIOModuleMonitor::at_MonitorResponseReceived, Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &QnIOModuleMonitor::at_MonitorMessageBodyAvailable, Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnIOModuleMonitor::at_MonitorConnectionClosed, Qt::DirectConnection);

    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
    m_multipartContentParser->setNextFilter(std::make_shared<QnMessageBodyParser>(this));

    QnMediaServerResourcePtr server = m_camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return false;
    if (m_camera->getStatus() < Qn::Online)
        return false;

    m_httpClient->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    QUrl requestUrl(server->getApiUrl());
    requestUrl.setPath(lit("/api/iomonitor"));
    QUrlQuery query;
    query.addQueryItem(lit("physicalId"), m_camera->getUniqueId());
    requestUrl.setQuery(query);

    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (!route.gatewayId.isNull()) {
        Q_ASSERT(!route.addr.isNull());
        requestUrl.setHost(route.addr.address.toString());
        requestUrl.setPort(route.addr.port);
    }

    m_httpClient->setUserName( QnAppServerConnectionFactory::url().userName().toLower() );
    m_httpClient->setUserPassword( QnAppServerConnectionFactory::url().password() );

    if (!m_httpClient->doGet( requestUrl ))
        return false;

    return true;
}

void QnIOModuleMonitor::at_MonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk( &m_mutex );
    if (httpClient != m_httpClient)
        return;

    if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( lit("Failed to subscribe to i/o monitor for camera %1. reason: %2").
            arg(m_camera->getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );
        return;
    }

    if( !m_multipartContentParser->setContentType(httpClient->contentType()) )
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        //unexpected content type
        NX_LOG( lit("Error monitoring IO port(s) on Axis camera %1. Unexpected Content-Type (%2) in monitor response. Expected: %3").
            arg(m_camera->getUrl()).arg(QLatin1String(httpClient->contentType())).arg(QLatin1String(multipartContentType)), cl_logWARNING );
        return;
    }

    lk.unlock();
    emit connectionOpened();
}

void QnIOModuleMonitor::at_MonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk(&m_mutex);
    if (httpClient == m_httpClient)
    {
        const nx_http::BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
        m_multipartContentParser->processData(msgBodyBuf);
    }
}

void QnIOModuleMonitor::at_MonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient )
{
    nx_http::AsyncHttpClientPtr httpClient;
    {
        QnMutexLocker lk(&m_mutex);
        if (httpClient != m_httpClient)
            return;

        std::swap(httpClient, m_httpClient);
    }

    emit connectionClosed();
}
