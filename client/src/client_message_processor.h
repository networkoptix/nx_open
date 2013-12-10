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

    virtual void init(const QUrl &url, const QString &authKey, int reconnectTimeout = EVENT_RECONNECT_TIMEOUT) override;
signals:
    void connectionOpened();
    void connectionClosed();

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

private slots:
    void at_messageReceived(QnMessage message);
    void at_connectionClosed(QString errorString);
    void at_connectionOpened(QnMessage message);
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
