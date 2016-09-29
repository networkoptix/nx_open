#include "fake_media_server.h"

void QnFakeMediaServerResource::setFakeServerModuleInformation(const ec2::ApiDiscoveredServerData& serverData)
{
    QnMutexLocker lock(&m_mutex);

    if (m_serverData == serverData)
        return;

    m_serverData = serverData;

    setId(serverData.id);
    setStatus(serverData.status, true);

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
    //setProperty(protoVersionPropertyName, QString::number(serverData.protoVersion));
    //setProperty(safeModePropertyName, QnLexical::serialized(serverData.ecDbReadOnly));

    if (serverData.ecDbReadOnly)
        addFlags(Qn::read_only);
    else
        removeFlags(Qn::read_only);

    emit moduleInformationChanged(toSharedPointer());
}

QnModuleInformation QnFakeMediaServerResource::getModuleInformation() const
{
    QnMutexLocker lock(&m_mutex);
    return m_serverData;
}

void QnMediaServerResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    NX_ASSERT(newStatus == Qn::Incompatible || newStatus == Qn::Unauthorized,
        Q_FUNC_INFO,
        "Incompatible servers should not take any status but incompatible or unauthorized");
    base_type::setStatus(newStatus, silenceMode);
}
