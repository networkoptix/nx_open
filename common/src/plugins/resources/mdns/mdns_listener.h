#ifndef __MDNS_LISTENER_H
#define __MDNS_LISTENER_H

#include <QTime>
#include <QList>
#include <QMap>
#include "utils/network/socket.h"

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

    typedef QMap<long, ConsumerDataList> ConsumersMap;

    void registerConsumer(long handle);
    ConsumerDataList getData(long handle);

    static QnMdnsListener* instance();

    QStringList getLocalAddressList() const;

private:
    void updateSocketList();
    void readDataFromSocket();
    void deleteSocketList();
    QString getBestLocalAddress(const QString& remoteAddress);
    void readSocketInternal(UDPSocket* socket, QString localAddress);

private:
    QList<UDPSocket*> m_socketList;
    UDPSocket* m_receiveSocket;
    QTime m_socketLifeTime;
    ConsumersMap m_data;
    QStringList m_localAddressList;
};

#endif // __MDNS_LISTENER_H
