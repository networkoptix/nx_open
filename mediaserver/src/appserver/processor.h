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
    QSet<QnResourcePtr> m_awaitingSetStatus;
    QMap<int, QnResourcePtr> m_handleToResource;
private:
    void updateResourceStatusAsync(const QnResourcePtr &resource);
    bool isSetStatusInProgress(const QnResourcePtr &resource);
private slots:
    void at_resource_statusChanged(const QnResourcePtr& resource);
    void requestFinished(const QnHTTPRawResponse& response, int handle);
};

#endif //_server_appserver_processor_h_
