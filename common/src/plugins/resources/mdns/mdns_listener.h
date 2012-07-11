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
        ConsumerData(const QByteArray& _response, const QString& _localAddress, const QString& _removeAddress):
                response(_response), localAddress(_localAddress), removeAddress(_removeAddress) {}

        QByteArray response;
        QString removeAddress;
        QString localAddress;
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
private:
    QList<UDPSocket*> m_socketList;
    QTime m_socketLifeTime;
    ConsumersMap m_data;
    QStringList m_localAddressList;
};

#endif // __MDNS_LISTENER_H
