// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "iomodule_monitor.h"

#include <QtCore/QUrlQuery>

#include <common/common_module.h>

#include <nx/utils/log/log.h>
#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "nx/fusion/serialization/json.h"
#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include "network/router.h"

namespace {
    int HTTP_READ_TIMEOUT = 1000 * 10;
}

class QnMessageBodyParser: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    QnMessageBodyParser(QnIOModuleMonitor* owner): m_owner(owner) {}
protected:
    virtual bool processData(const nx::ConstBufferRefType& data) override
    {
        bool ok = false;
        QnIOStateDataList valueList = QJson::deserialized<QnIOStateDataList>(data, QnIOStateDataList(), &ok);
        if (ok) {
            for (const auto& value: valueList)
                emit m_owner->ioStateChanged(value);
        }
        else
        {
            NX_WARNING(this, "Invalid data to deserialize IO state: %1", data.toRawByteArray());
        }
        return true;
    }
private:
    QnIOModuleMonitor* m_owner;
};

QnIOModuleMonitor::QnIOModuleMonitor(const QnSecurityCamResourcePtr &camera):
    m_camera(camera)
{
}

QnIOModuleMonitor::~QnIOModuleMonitor()
{
    if (m_httpClient)
    {
        m_httpClient->disconnect(this);
        m_httpClient->pleaseStopSync();
        m_httpClient.reset();
    }
}

bool QnIOModuleMonitor::open()
{
    {
        nx::network::http::AsyncHttpClientPtr httpClient;
        NX_MUTEX_LOCKER lk(&m_mutex);
        std::swap(httpClient, m_httpClient);
    }

    QnMediaServerResourcePtr server = m_camera->getParentServer();
    if (!server)
        return false;

    m_httpClient =
        nx::network::http::AsyncHttpClient::create(nx::network::ssl::kAcceptAnyCertificate);
    m_httpClient->setResponseReadTimeoutMs(HTTP_READ_TIMEOUT);
    m_httpClient->setMessageBodyReadTimeoutMs(HTTP_READ_TIMEOUT);

    connect(
        m_httpClient.get(), &nx::network::http::AsyncHttpClient::responseReceived,
        this, &QnIOModuleMonitor::at_MonitorResponseReceived, Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx::network::http::AsyncHttpClient::someMessageBodyAvailable,
        this, &QnIOModuleMonitor::at_MonitorMessageBodyAvailable, Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &QnIOModuleMonitor::at_MonitorConnectionClosed, Qt::DirectConnection);

    m_multipartContentParser = std::make_shared<nx::network::http::MultipartContentParser>();
    m_multipartContentParser->setNextFilter(std::make_shared<QnMessageBodyParser>(this));

    if (!m_camera->isOnline())
        return false;

    m_httpClient->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, server->getId().toStdString());
    nx::utils::Url requestUrl(server->getApiUrl());
    requestUrl.setPath(lit("/api/iomonitor"));
    QUrlQuery query;
    query.addQueryItem(QString::fromLatin1(Qn::PHYSICAL_ID_URL_QUERY_ITEM), m_camera->getUniqueId());
    requestUrl.setQuery(query);

    QnRoute route = commonModule()->router()->routeTo(server->getId());
    if (!route.gatewayId.isNull())
    {
        if (route.addr.isNull())
        {
            NX_WARNING(this, "Can't detect server IP address to open IO monitor connection.");
            return false; //< No primary interface have found for media server yet. Try to connect later.
        }
        requestUrl.setHost(route.addr.address.toString());
        requestUrl.setPort(route.addr.port);
    }

    m_httpClient->setCredentials(connectionCredentials());
    m_httpClient->doGet(requestUrl);
    return true;
}

void QnIOModuleMonitor::at_MonitorResponseReceived( nx::network::http::AsyncHttpClientPtr httpClient )
{
    NX_MUTEX_LOCKER lk( &m_mutex );
    if (httpClient != m_httpClient)
        return;

    if( httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::ok )
    {
        NX_WARNING(this, "Failed to subscribe to i/o monitor for camera %1. reason: %2",
            m_camera->getUrl(), httpClient->response()->statusLine.reasonPhrase);
        return;
    }

    if( !m_multipartContentParser->setContentType(httpClient->contentType()) )
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        // Unexpected content type.
        NX_WARNING(this, "Error monitoring IO ports on camera %1. Unexpected Content-Type (%2) in "
            "monitor response. Expected: %3",
            m_camera->getUrl(), httpClient->contentType(), multipartContentType);
        return;
    }

    lk.unlock();
    emit connectionOpened();
}

void QnIOModuleMonitor::at_MonitorMessageBodyAvailable( nx::network::http::AsyncHttpClientPtr httpClient )
{
    NX_ASSERT( httpClient );
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (httpClient == m_httpClient)
    {
        const auto& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
        m_multipartContentParser->processData(msgBodyBuf);
    }
}

void QnIOModuleMonitor::at_MonitorConnectionClosed( nx::network::http::AsyncHttpClientPtr httpClient )
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (httpClient != m_httpClient)
            return;
        m_multipartContentParser->flush();
        m_httpClient.reset();
    }

    emit connectionClosed();
}
