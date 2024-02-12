// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fake_media_server.h"

#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>

QnFakeMediaServerResource::QnFakeMediaServerResource():
    base_type()
{
    setIdUnsafe(nx::Uuid::createUuid());
    addFlags(Qn::fake_server);
}

nx::Uuid QnFakeMediaServerResource::getOriginalGuid() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_serverData.id;
}

void QnFakeMediaServerResource::setFakeServerModuleInformation(
    const nx::vms::api::DiscoveredServerData& serverData)
{
    nx::vms::api::DiscoveredServerData oldData;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_serverData == serverData)
            return;
        oldData = m_serverData;
        m_serverData = serverData;
    }

    if (serverData.status != oldData.status)
        emit statusChanged(toSharedPointer(this), Qn::StatusChangeReason::Local);

    QList<nx::network::SocketAddress> addressList;
    for (QString address: serverData.remoteAddresses)
    {
        const bool isIpV6 = address.count(':') > 1;
        if (isIpV6 && !address.startsWith('['))
            address = '[' + address + ']';

        addressList.append(nx::network::SocketAddress(address.toStdString()));
    }
    setNetAddrList(addressList);

    if (!addressList.isEmpty())
    {
        nx::network::SocketAddress endpoint(addressList.first());
        if (endpoint.port == 0)
            endpoint.port = serverData.port;

        const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(endpoint);
        setUrl(url.toUrl().toString());
    }

    if (!serverData.name.isEmpty() && serverData.name != oldData.name)
        emit nameChanged(toSharedPointer(this));

    setVersion(serverData.version);
}

nx::vms::api::ModuleInformation QnFakeMediaServerResource::getModuleInformation() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_serverData;
}

QString QnFakeMediaServerResource::getName() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_serverData.name;
}

nx::vms::api::ResourceStatus QnFakeMediaServerResource::getStatus() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return static_cast<nx::vms::api::ResourceStatus>(m_serverData.status);
}

nx::utils::Url QnFakeMediaServerResource::getApiUrl() const
{
    auto url = base_type::getApiUrl();
    url.setUserName(m_authenticator.user());
    url.setPassword(m_authenticator.password());
    return url;
}

void QnFakeMediaServerResource::setAuthenticator(const QAuthenticator& authenticator)
{
    m_authenticator = authenticator;
}

void QnFakeMediaServerResource::updateInternal(
    const QnResourcePtr& /*source*/,
    NotifierList& /*notifiers*/)
{
    NX_ASSERT(false, "This function should be not used for fake media servers");
}
