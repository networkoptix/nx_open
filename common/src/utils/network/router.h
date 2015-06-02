#ifndef ROUTER_H
#define ROUTER_H

#include <QtCore/QObject>
#include <utils/thread/mutex.h>

#include <utils/common/singleton.h>
#include <utils/network/http/httptypes.h>
#include <utils/network/socket_common.h>

class QnModuleFinder;

struct QnRoute
{
    SocketAddress addr; // address for physical connect
    QnUuid id;          // requested server ID
    QnUuid gatewayId;   // proxy server ID. May be null
    bool reverseConnect;// if target server should connect to this one

    bool isValid() const { return !addr.isNull(); }
    QnRoute() : reverseConnect(false) {}
};

class QnRouter : public QObject, public Singleton<QnRouter> {
    Q_OBJECT
public:
    explicit QnRouter(QnModuleFinder *moduleFinder, QObject *parent = 0);
    ~QnRouter();

    // todo: new routing functions below. We have to delete above functions
    QnRoute routeTo(const QnUuid &id);
    void updateRequest(QUrl& url, nx_http::HttpHeaders& headers, const QnUuid &id);
private:
    const QnModuleFinder *m_moduleFinder;
};

#endif // ROUTER_H
