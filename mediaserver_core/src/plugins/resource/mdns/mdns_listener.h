#ifndef __MDNS_LISTENER_H
#define __MDNS_LISTENER_H

#ifdef ENABLE_MDNS

#include <list>

#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QStringList>

#include "utils/network/socket.h"


class UDPSocket;

class QnMdnsListener
{
public:
    QnMdnsListener();
    ~QnMdnsListener();

    struct ConsumerData
    {
        ConsumerData(const QByteArray& response, const QString& localAddress, const QString& remoteAddress):
                response(response), localAddress(localAddress), remoteAddress(remoteAddress) {}

        QByteArray response;
        QString localAddress;
        QString remoteAddress;
    };

    typedef QList<ConsumerData> ConsumerDataList;

    typedef QMap<std::uintptr_t, ConsumerDataList> ConsumersMap;

    void registerConsumer(std::uintptr_t handle);
    ConsumerDataList getData(std::uintptr_t handle);

    static QnMdnsListener* instance();

    QStringList getLocalAddressList() const;

private:
    void updateSocketList();
    void readDataFromSocket();
    void deleteSocketList();
    QString getBestLocalAddress(const QString& remoteAddress);
    void readSocketInternal(AbstractDatagramSocket* socket, QString localAddress);

private:
    QList<AbstractDatagramSocket*> m_socketList;
    AbstractDatagramSocket* m_receiveSocket;
    QElapsedTimer m_socketLifeTime;
    //!list<pair<consumer id, consumer data> >. List is required to garantee, that consumers receive data in order they were registered
    std::list<std::pair<std::uintptr_t, ConsumerDataList> > m_data;
    QStringList m_localAddressList;
};

#endif // ENABLE_MDNS

#endif // __MDNS_LISTENER_H
