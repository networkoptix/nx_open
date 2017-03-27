#ifndef ROUTER_H
#define ROUTER_H

#include <QtCore/QObject>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/singleton.h>
#include <nx/network/http/httptypes.h>
#include <nx/network/socket_common.h>
#include <common/common_module_aware.h>

class QnModuleFinder;

struct QnRoute
{

    SocketAddress addr; // address for physical connect
    QnUuid id;          // requested server ID
    QnUuid gatewayId;   // proxy server ID. May be null
    bool reverseConnect;// if target server should connect to this one
    int distance;

    QnRoute(): reverseConnect(false), distance(0) {}
    bool isValid() const { return !addr.isNull(); }
};

class QnRouter: public QnCommonModuleAware
{
    Q_OBJECT
public:
    explicit QnRouter(
        QObject* parent,
        QnModuleFinder *moduleFinder);
    ~QnRouter();

    // todo: new routing functions below. We have to delete above functions
    QnRoute routeTo(const QnUuid &id);
    void updateRequest(QUrl& url, nx_http::HttpHeaders& headers, const QnUuid &id);
private:
    const QnModuleFinder *m_moduleFinder;
};

#endif // ROUTER_H
