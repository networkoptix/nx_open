#include "fake_media_server.h"

#include <nx/network/url/url_builder.h>

QnFakeMediaServerResource::QnFakeMediaServerResource(QnCommonModule* commonModule):
    QnMediaServerResource(commonModule)
{
    setId(QnUuid::createUuid());
    addFlags(Qn::fake_server);
}

QnUuid QnFakeMediaServerResource::getOriginalGuid() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serverData.id;
}

void QnFakeMediaServerResource::setFakeServerModuleInformation(
    const nx::vms::api::DiscoveredServerData& serverData)
{
    nx::vms::api::DiscoveredServerData oldData;
    {
        QnMutexLocker lock(&m_mutex);
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

        addressList.append(nx::network::SocketAddress(address));
    }
    setNetAddrList(addressList);

    if (!addressList.isEmpty())
    {
        nx::network::SocketAddress endpoint(addressList.first());
        if (endpoint.port == 0)
            endpoint.port = serverData.port;

        const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::urlSheme(serverData.sslAllowed))
            .setEndpoint(endpoint);
        setUrl(url.toString());
    }

    if (!serverData.name.isEmpty() && serverData.name != oldData.name)
        emit nameChanged(toSharedPointer(this));

    setVersion(serverData.version);
    setSystemInfo(serverData.systemInformation);
    setSslAllowed(serverData.sslAllowed);

    if (serverData.ecDbReadOnly)
        addFlags(Qn::read_only);
    else
        removeFlags(Qn::read_only);

    emit moduleInformationChanged(::toSharedPointer(this));
}

nx::vms::api::ModuleInformation QnFakeMediaServerResource::getModuleInformation() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serverData;
}

QString QnFakeMediaServerResource::getName() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serverData.name;
}

Qn::ResourceStatus QnFakeMediaServerResource::getStatus() const
{
    QnMutexLocker lock(&m_mutex);
    return static_cast<Qn::ResourceStatus>(m_serverData.status);
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
    apiConnection()->setUrl(getApiUrl());
}

void QnFakeMediaServerResource::updateInternal(const QnResourcePtr& /*other*/, Qn::NotifierList& /*notifiers*/)
{
    NX_ASSERT(false, "This function should be not used for fake media servers");
}
