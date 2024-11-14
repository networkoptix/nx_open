// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_module_monitor.h"

#include <QtCore/QUrlQuery>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <network/router.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/system_context.h>


namespace nx::vms::client::core {

using namespace nx::network::http;

namespace {

static constexpr int kHttpReadTimeout = 1000 * 10;

class MessageBodyParser: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    MessageBodyParser(IOModuleMonitor* owner);

private:
    virtual bool processData(const nx::ConstBufferRefType& data) override;

    IOModuleMonitor* m_owner;
};

MessageBodyParser::MessageBodyParser(IOModuleMonitor* owner):
    m_owner(owner)
{
}

bool MessageBodyParser::processData(const nx::ConstBufferRefType& data)
{
    bool ok = false;
    const auto& valueList = QJson::deserialized<QnIOStateDataList>(data, QnIOStateDataList{}, &ok);
    if (ok)
    {
        for (const auto& value: valueList)
            emit m_owner->ioStateChanged(value);
    }
    else
    {
        NX_WARNING(this, "Invalid data to deserialize IO state: %1", data.toRawByteArray());
    }
    return true;
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct IOModuleMonitor::Private
{
    IOModuleMonitor * const q;

    mutable nx::Mutex mutex;
    QnVirtualCameraResourcePtr camera;
    AsyncHttpClientPtr httpClient;
    std::shared_ptr<MultipartContentParser> multipartContentParser;
    std::atomic_bool connectionIsOpened = false;

    void handleResponseReceived(const AsyncHttpClientPtr& client);
    void handleMessageBodyAvailable(const AsyncHttpClientPtr& client);
    void handleConnectionClosed(const AsyncHttpClientPtr& client);
};

void IOModuleMonitor::Private::handleResponseReceived(const AsyncHttpClientPtr& client)
{
    NX_MUTEX_LOCKER lk(&mutex);
    if (client != httpClient)
        return;

    if (client->response()->statusLine.statusCode != StatusCode::ok)
    {
        NX_WARNING(this, "Failed to subscribe to i/o monitor for camera %1. reason: %2",
            camera->getUrl(), client->response()->statusLine.reasonPhrase);

        return;
    }

    if (!multipartContentParser->setContentType(client->contentType()))
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        // Unexpected content type.
        NX_WARNING(this, "Error monitoring IO ports on camera %1. Unexpected Content-Type (%2) in "
            "monitor response. Expected: %3", camera->getUrl(), client->contentType(),
            multipartContentType);

        return;
    }

    lk.unlock();

    connectionIsOpened = true;

    emit q->connectionOpened();
}

void IOModuleMonitor::Private::handleMessageBodyAvailable(const AsyncHttpClientPtr& client)
{
    NX_ASSERT(client);
    NX_MUTEX_LOCKER lk(&mutex);
    if (client != httpClient)
        return;

    const auto& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
    multipartContentParser->processData(msgBodyBuf);
}

void IOModuleMonitor::Private::handleConnectionClosed(const AsyncHttpClientPtr& client)
{
    {
        NX_MUTEX_LOCKER lk(&mutex);
        if (client != httpClient)
            return;

        multipartContentParser->flush();
        httpClient.reset();
    }

    connectionIsOpened = false;
    emit q->connectionClosed();
}

//-------------------------------------------------------------------------------------------------

IOModuleMonitor::IOModuleMonitor(const QnVirtualCameraResourcePtr &camera, QObject* parent):
    base_type(parent),
    d(new Private{.q = this, .camera = camera})
{
}

IOModuleMonitor::~IOModuleMonitor()
{
    if (!d->httpClient)
        return;

    d->httpClient->disconnect(this);
    d->httpClient->pleaseStopSync();
    d->httpClient.reset();
}

bool IOModuleMonitor::open()
{
    {
        nx::network::http::AsyncHttpClientPtr httpClient;
        NX_MUTEX_LOCKER lk(&d->mutex);
        std::swap(httpClient, d->httpClient);
    }

    if (!d->camera->isOnline())
        return false;

    QnMediaServerResourcePtr server = d->camera->getParentServer();
    if (!server)
        return false;

    auto systemContext = SystemContext::fromResource(d->camera);
    if (!NX_ASSERT(systemContext) || !systemContext->connection())
        return false;

    d->httpClient =
        nx::network::http::AsyncHttpClient::create(nx::network::ssl::kAcceptAnyCertificate);
    d->httpClient->setResponseReadTimeoutMs(kHttpReadTimeout);
    d->httpClient->setMessageBodyReadTimeoutMs(kHttpReadTimeout);

    connect(d->httpClient.get(), &nx::network::http::AsyncHttpClient::responseReceived,
        this, [this](const AsyncHttpClientPtr& client) { d->handleResponseReceived(client); });

    connect(d->httpClient.get(), &nx::network::http::AsyncHttpClient::someMessageBodyAvailable,
        this, [this](const AsyncHttpClientPtr& client) { d->handleMessageBodyAvailable(client); });

    connect(d->httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, [this](const AsyncHttpClientPtr& client) { d->handleConnectionClosed(client); });

    d->multipartContentParser = std::make_shared<nx::network::http::MultipartContentParser>();
    d->multipartContentParser->setNextFilter(std::make_shared<MessageBodyParser>(this));

    d->httpClient->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, server->getId().toStdString());
    nx::utils::Url requestUrl(server->getApiUrl());
    requestUrl.setPath(lit("/api/iomonitor"));
    QUrlQuery query;
    query.addQueryItem(QString::fromLatin1(Qn::PHYSICAL_ID_URL_QUERY_ITEM), d->camera->getPhysicalId());
    requestUrl.setQuery(query);

    QnRoute route = QnRouter::routeTo(server);
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

    d->httpClient->setCredentials(systemContext->connectionCredentials());
    d->httpClient->doGet(requestUrl);
    return true;
}

bool IOModuleMonitor::connectionIsOpened() const
{
    return d->connectionIsOpened;
}

} // namespace nx::vms::client::core
