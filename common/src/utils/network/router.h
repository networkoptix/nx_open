#ifndef ROUTER_H
#define ROUTER_H

#include <QtCore/QObject>
#include <utils/thread/mutex.h>

#include <utils/common/singleton.h>
#include "socket_common.h"

class QnModuleFinder;

struct QnRoute
{
    SocketAddress addr; // address for physical connect
    QnUuid id;          // requested server ID
    QnUuid gatewayId;   // proxy server ID. May be null

    bool isValid() const { return !addr.isNull(); }
};

class QnRouter : public QObject, public Singleton<QnRouter> {
    Q_OBJECT
public:
    explicit QnRouter(QnModuleFinder *moduleFinder, QObject *parent = 0);
    ~QnRouter();

    // todo: new routing functions below. We have to delete above functions
    QnRoute routeTo(const QnUuid &id);
private:
    const QnModuleFinder *m_moduleFinder;
};

#endif // ROUTER_H
