#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include "core/resource/resource.h"
#include "api/AppServerConnection.h"

class QnAppserverResourceProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT

public:
    QnAppserverResourceProcessor(QnId serverId);

    void processResources(const QnResourceList &resources);

private:
    QnAppServerConnectionPtr m_appServer;
    QnId m_serverId;

private slots:
    void onResourceStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus);
    void requestFinished(int status, const QByteArray& errorString, const QnResourceList& resources, int handle);
};

#endif //_server_appserver_processor_h_
