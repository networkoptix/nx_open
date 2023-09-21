// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_processor.h"

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/url/url_builder.h>

#include "network_manager.h"
#include "certificate_verifier.h"

namespace nx::vms::client::core {

struct QueryProcessor::Private
{
    Private(
        const QnUuid& serverId,
        const QnUuid& runningInstanceId,
        AbstractCertificateVerifier* certificateVerifier,
        nx::network::SocketAddress address,
        nx::network::http::Credentials credentials,
        Qn::SerializationFormat format)
        :
        serverId(serverId),
        runningInstanceId(runningInstanceId),
        certificateVerifier(certificateVerifier),
        address(std::move(address)),
        credentials(std::move(credentials)),
        serializationFormat(format),
        networkManager(new NetworkManager())
    {
    }

    std::unique_ptr<nx::network::http::AsyncClient> makeHttpClient() const
    {
        auto httpClient = std::make_unique<nx::network::http::AsyncClient>(
            certificateVerifier->makeAdapterFunc(serverId, address));
        NetworkManager::setDefaultTimeouts(httpClient.get());

        // This header is used by the server to identify the client login session for audit.
        httpClient->addAdditionalHeader(
            Qn::EC2_RUNTIME_GUID_HEADER_NAME, runningInstanceId.toStdString());

        httpClient->setCredentials(credentials);

        // Allow to update realm in case of migration.
        httpClient->addAdditionalHeader(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME, {});

        return httpClient;
    }

    nx::utils::Url makeUrl(ec2::ApiCommand::Value cmdCode) const
    {
        NX_MUTEX_LOCKER lk(&mutex);
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(address)
            .setPath("/ec2/" + ec2::ApiCommand::toString(cmdCode))
            .toUrl();
    }

    mutable nx::Mutex mutex;

    const QnUuid serverId;
    QnUuid runningInstanceId;
    AbstractCertificateVerifier* certificateVerifier;
    nx::network::SocketAddress address;
    nx::network::http::Credentials credentials;
    const Qn::SerializationFormat serializationFormat;
    std::unique_ptr<NetworkManager> networkManager;
};

QueryProcessor::QueryProcessor(
    const QnUuid& serverId,
    const QnUuid& runningInstanceId,
    AbstractCertificateVerifier* certificateVerifier,
    nx::network::SocketAddress address,
    nx::network::http::Credentials credentials,
    Qn::SerializationFormat serializationFormat)
    :
    d(new Private(
        serverId,
        runningInstanceId,
        certificateVerifier,
        std::move(address),
        std::move(credentials),
        serializationFormat))
{
}

void QueryProcessor::updateSessionId(const QnUuid& sessionId)
{
    d->runningInstanceId = sessionId;
}

QueryProcessor::~QueryProcessor()
{
    pleaseStopSync();
}

void QueryProcessor::pleaseStopSync()
{
    d->networkManager->pleaseStopSync();
}

nx::network::SocketAddress QueryProcessor::address() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->address;
}

void QueryProcessor::updateAddress(nx::network::SocketAddress value)
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->address = std::move(value);
}

nx::network::http::Credentials QueryProcessor::credentials() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->credentials;
}

void QueryProcessor::updateCredentials(nx::network::http::Credentials value)
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->credentials = std::move(value);
}

nx::network::ssl::AdapterFunc QueryProcessor::adapterFunc(const QnUuid& serverId) const
{
    return d->certificateVerifier->makeAdapterFunc(serverId, d->address);
}

Qn::SerializationFormat QueryProcessor::serializationFormat() const
{
    return d->serializationFormat;
}

void QueryProcessor::sendGetRequest(
    ec2::ApiCommand::Value cmdCode,
    QUrlQuery query,
    std::function<void(ec2::ErrorCode, Qn::SerializationFormat, const nx::Buffer&)> handler)
{
    auto httpClient = d->makeHttpClient();

    nx::utils::Url requestUrl(d->makeUrl(cmdCode));

    query.addQueryItem("format", nx::toString(d->serializationFormat));
    requestUrl.setQuery(query);

    d->networkManager->doGet(std::move(httpClient), requestUrl, this,
        [handler](NetworkManager::Response response)
        {
            handler(
                response.errorCode,
                Qn::serializationFormatFromHttpContentType(response.contentType),
                response.messageBody);
        },
        Qt::DirectConnection);
}

void QueryProcessor::sendPostRequest(
    ec2::ApiCommand::Value cmdCode,
    QByteArray serializedData,
    std::function<void(ec2::ErrorCode)> handler)
{
    auto httpClient = d->makeHttpClient();

    auto messageBody = std::make_unique<nx::network::http::BufferSource>(
        Qn::serializationFormatToHttpContentType(d->serializationFormat),
        std::move(serializedData));
    httpClient->setRequestBody(std::move(messageBody));

    d->networkManager->doPost(
        std::move(httpClient),
        d->makeUrl(cmdCode),
        this,
        [handler](NetworkManager::Response response) { handler(response.errorCode); },
        Qt::DirectConnection);
}

} // namespace nx::vms::client::core
