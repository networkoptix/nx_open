#ifndef __MSERVER_RESOURCE_SEARCHER_H__
#define __MSERVER_RESOURCE_SEARCHER_H__

#include <memory>

#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QByteArray>
#include <QtCore/QTime>

#include "core/resource_management/resource_searcher.h"
#include "nx/utils/thread/long_runnable.h"
#include <nx/network/socket.h>
#include "common/common_module_aware.h"


namespace nx {
namespace network {
class UDPSocket;
}   //network
}   //nx
struct QnCameraConflictList;

class QnMServerResourceSearcher: public QnLongRunnable, public QnCommonModuleAware
{
public:
    QnMServerResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnMServerResourceSearcher();

    /** find other servers in current networks. Actually, this function do not instantiate other mServer as resources. Function just check if they are presents */
    virtual void run() override;
private:
    void updateSocketList();
    void deleteSocketList();
    void readDataFromSocket();
    void readSocketInternal(nx::network::AbstractDatagramSocket* socket, QnCameraConflictList& conflictList);
private:
    QStringList m_localAddressList;
    QList<nx::network::AbstractDatagramSocket*> m_socketList;
    QTime m_socketLifeTime;
    std::unique_ptr<nx::network::AbstractDatagramSocket> m_receiveSocket;
};

#endif // __MSERVER_RESOURCE_SEARCHER_H__
