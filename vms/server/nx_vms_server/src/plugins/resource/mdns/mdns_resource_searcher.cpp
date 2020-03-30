#include "mdns_resource_searcher.h"

#ifdef ENABLE_MDNS

#include <nx/network/nettools.h>

#include "mdns_listener.h"
#include <media_server/media_server_module.h>
#include "mdns_packet.h"

namespace {

    static const QChar kSeparator = '.';

    int getSimilarity(const QString& remoteAddress, const QString& localAddress)
    {
        const auto remoteParts = remoteAddress.splitRef(kSeparator);
        const auto localParts = localAddress.splitRef(kSeparator);
        int result = 0;
        for (int i = 0; i < localParts.size(); ++i)
        {
            if (remoteParts.size() < i || remoteParts[i] != localParts[i])
                break;
            ++result;
        }
        return result;
    }

    QString extractIpFromPtrRecordName(const QString& ptrRecord)
    {
        const auto data = ptrRecord.split(kSeparator);
        if (data.size() < 4)
            return QString();
        return data[3] + kSeparator + data[2] + kSeparator + data[1] + kSeparator + data[0];
    }

    QString getDeviceAddress(const QList<nx::network::HostAddress>& localAddressList,
        const QString& remoteAddress, const QByteArray& responseData)
    {
        QString result = remoteAddress;
        QnMdnsPacket packet;
        if (!packet.fromDatagram(responseData))
            return result;

        QStringList remoteAddressList;
        for (const auto& record : packet.answerRRs)
        {
            if (record.recordType == QnMdnsPacket::kPtrRecordType
                && record.recordName.contains("in-addr.arpa"))
            {
                const auto remoteAddress = extractIpFromPtrRecordName(record.recordName);
                if (!remoteAddress.isEmpty())
                    remoteAddressList << remoteAddress;
            }
        }
        if (remoteAddressList.empty())
            return remoteAddress;

        int similarity = 0;
        for (const auto& localAddress: localAddressList)
        {
            for (const auto& remoteAddress: remoteAddressList)
            {
                const int newSimilarity = getSimilarity(localAddress.toString(), remoteAddress);
                if (newSimilarity > similarity)
                {
                    similarity = newSimilarity;
                    result = remoteAddress;
                }
            }
        }
        return result;
    }

} // namespace

QnMdnsResourceSearcher::QnMdnsResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    m_serverModule(serverModule)
{
    serverModule->mdnsListener()->registerConsumer((std::uintptr_t) this);
}

QnMdnsResourceSearcher::~QnMdnsResourceSearcher()
{

}

QnResourceList QnMdnsResourceSearcher::findResources()
{
    QnResourceList result;

    const auto addressList = allLocalAddresses(nx::network::AddressFilter::onlyFirstIpV4);
    auto consumerData = m_serverModule->mdnsListener()->getData((std::uintptr_t) this);
    consumerData->forEachEntry(
        [this, addressList, &result](
            const QString& remoteAddress,
            const QString& localAddress,
            const QByteArray& responseData)
        {
        const auto deviceAddress = getDeviceAddress(addressList, remoteAddress, responseData);
        const QList<QnNetworkResourcePtr>& nresourceLst = processPacket(
            result,
            responseData,
            QHostAddress(localAddress),
            QHostAddress(deviceAddress));

            for(const QnNetworkResourcePtr& nresource: nresourceLst)
            {
                nresource->setHostAddress(deviceAddress);
                result.push_back(nresource);
            }
        });

    return result;
}

#endif // ENABLE_MDNS
