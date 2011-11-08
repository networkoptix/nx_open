#ifndef _server_appserver_server_h_
#define _server_appserver_server_h_

#include <QSharedPointer>
#include <QList>

#include "core/resource/network_resource.h"

class QnServer : public QnNetworkResource
{
public:
    QnServer(const QString& name);

    QString getUniqueId() const;
    bool isResourceAccessible();
    bool updateMACAddress();
    QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
};

typedef QSharedPointer<QnServer> QnServerPtr;
typedef QList<QnServerPtr> QnServerList;

#endif // _server_appserver_server_h_
