#ifndef __MSERVER_RESOURCE_SEARCHER_H__
#define __MSERVER_RESOURCE_SEARCHER_H__

#include <QString>
#include <QSet>
#include <QByteArray>

#include "core/resource_managment/resource_searcher.h"
#include "utils/common/long_runnable.h"
#include "utils/network/socket.h"

class QnMServerResourceSearcher : public QnLongRunnable
{
public:
    QnMServerResourceSearcher();
    virtual ~QnMServerResourceSearcher();

    static QnMServerResourceSearcher* instance();
    void setAppPServerGuid(const QByteArray& appServerGuid);
    /** find other media servers in current networks. Actually, this function do not instantiate other mServer as resources. Function just check if they are presents */
    virtual void run() override;
private:
    void updateSocketList();
    void deleteSocketList();
    void readDataFromSocket();
    void readSocketInternal(UDPSocket* socket, QSet<QByteArray>& conflictList);
private:
    QStringList m_localAddressList;
    QList<UDPSocket*> m_socketList;
    QTime m_socketLifeTime;
    UDPSocket* m_receiveSocket;
    QByteArray m_appServerGuid;
};

#endif // __MSERVER_RESOURCE_SEARCHER_H__
