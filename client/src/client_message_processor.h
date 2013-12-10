#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <QSharedPointer>

#include "api/app_server_connection.h"
#include "core/resource/resource.h"

#include <api/common_message_processor.h>

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnClientMessageProcessor();

    virtual void run() override;
protected:
    virtual void loadRuntimeInfo(const QnMessage &message) override;
    virtual void handleConnectionOpened(const QnMessage &message) override;
    virtual void handleMessage(const QnMessage &message) override;

private slots:

    void at_serverIfFound(const QnMediaServerResourcePtr &resource, const QString & url, const QString& origApiUrl);

private:
    void init();
    void determineOptimalIF(QnMediaServerResource* mediaServer);
    bool updateResource(QnResourcePtr resource, bool insert = true);
    void processResources(const QnResourceList& resources);
    void processLicenses(const QnLicenseList& licenses);
    void processCameraServerItems(const QnCameraHistoryList& cameraHistoryList);
    void updateHardwareIds(const QnMessage& message);

private:
    quint32 m_seqNumber;
};

#endif // _client_event_manager_h
