#pragma once

#ifdef ENABLE_MDNS

#include <stdint.h>
#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QStringList>

#include <nx/network/socket.h>
#include "mdns_consumer_data.h"


namespace nx {
namespace network {
class UDPSocket;
}   //network
}   //nx


class QnMdnsListener
{
public:
    QnMdnsListener();
    ~QnMdnsListener();

    void registerConsumer(uintptr_t id);
    const ConsumerData *getData(uintptr_t id);

    static QnMdnsListener* instance();

    QStringList getLocalAddressList() const;

private:
    static const std::chrono::seconds kRefreshTimeout;

    QList<nx::network::AbstractDatagramSocket*> m_socketList;
    nx::network::AbstractDatagramSocket* m_receiveSocket;
    QElapsedTimer m_socketLifeTime;
    QStringList m_localAddressList;
    detail::ConsumerDataListPtr m_consumersData;
    std::chrono::time_point<std::chrono::steady_clock> m_lastRefreshTime;

    void updateSocketList();
    void readDataFromSocket();
    void deleteSocketList();
    QString getBestLocalAddress(const QString& remoteAddress);
    void readSocketInternal(nx::network::AbstractDatagramSocket* socket, const QString& localAddress);
    bool needRefresh() const;
};

#endif // ENABLE_MDNS
