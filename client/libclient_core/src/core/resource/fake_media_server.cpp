#include "fake_media_server.h"

QnFakeMediaServerResource::QnFakeMediaServerResource():
    QnMediaServerResource()
{
    setId(QnUuid::createUuid());
    addFlags(Qn::fake_server);
}

QnUuid QnFakeMediaServerResource::getOriginalGuid() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serverData.id;
}

void QnFakeMediaServerResource::setFakeServerModuleInformation(const ec2::ApiDiscoveredServerData& serverData)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_serverData == serverData)
            return;
        m_serverData = serverData;
    }

    setStatus(serverData.status, Qn::StatusChangeReason::Local);

    QList<SocketAddress> addressList;
    for (const QString &address : serverData.remoteAddresses)
        addressList.append(SocketAddress(address));
    setNetAddrList(addressList);

    if (!addressList.isEmpty())
    {
        const SocketAddress address(addressList.first().toString(), serverData.port);
        const auto url = address.toUrl(apiUrlScheme(serverData.sslAllowed)).toString();
        setUrl(url);
    }
    if (!serverData.name.isEmpty())
        setName(serverData.name);
    setVersion(serverData.version);
    setSystemInfo(serverData.systemInformation);
    setSslAllowed(serverData.sslAllowed);

    if (serverData.ecDbReadOnly)
        addFlags(Qn::read_only);
    else
        removeFlags(Qn::read_only);

    emit moduleInformationChanged(::toSharedPointer(this));
}

QnModuleInformation QnFakeMediaServerResource::getModuleInformation() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serverData;
}

void QnFakeMediaServerResource::setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    NX_ASSERT(newStatus == Qn::Incompatible || newStatus == Qn::Unauthorized,
        "Incompatible servers should not take any status but incompatible or unauthorized");
    base_type::setStatus(newStatus, reason);
}

QUrl QnFakeMediaServerResource::getApiUrl() const
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
    Q_ASSERT("This function should be not used for fake media servers");
}
