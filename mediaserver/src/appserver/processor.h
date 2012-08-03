#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include "core/resource/resource.h"
#include "api/app_server_connection.h"

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
    void onResourceStatusChanged(const QnResourcePtr& resource);
    void requestFinished(int status, const QByteArray &data, const QByteArray& errorString, int handle);
};

#endif //_server_appserver_processor_h_
