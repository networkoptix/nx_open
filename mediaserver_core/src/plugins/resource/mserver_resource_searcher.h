#ifndef __MSERVER_RESOURCE_SEARCHER_H__
#define __MSERVER_RESOURCE_SEARCHER_H__

#include <memory>

#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QByteArray>
#include <QtCore/QTime>

#include "core/resource_management/resource_searcher.h"
#include "utils/common/long_runnable.h"
#include <nx/network/socket.h>


namespace nx {
namespace network {
class UDPSocket;
}   //network
}   //nx
struct QnCameraConflictList;

class QnMServerResourceSearcher : public QnLongRunnable
{
public:
    QnMServerResourceSearcher();
    virtual ~QnMServerResourceSearcher();

    static void initStaticInstance( QnMServerResourceSearcher* inst );
    static QnMServerResourceSearcher* instance();
    /** find other servers in current networks. Actually, this function do not instantiate other mServer as resources. Function just check if they are presents */
    virtual void run() override;
private:
    void updateSocketList();
    void deleteSocketList();
    void readDataFromSocket();
    void readSocketInternal(AbstractDatagramSocket* socket, QnCameraConflictList& conflictList);
private:
    QStringList m_localAddressList;
    QList<AbstractDatagramSocket*> m_socketList;
    QTime m_socketLifeTime;
    std::unique_ptr<AbstractDatagramSocket> m_receiveSocket;
};

#endif // __MSERVER_RESOURCE_SEARCHER_H__
