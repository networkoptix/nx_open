#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <QSharedPointer>

#include "api/message_source.h"
#include "api/app_server_connection.h"
#include "core/resource/resource.h"

class QnClientMessageProcessor : public QObject
{
    Q_OBJECT

public:
    static QnClientMessageProcessor *instance();

    QnClientMessageProcessor();

    void stop();

signals:
    void connectionOpened();
    void connectionClosed();

    void businessRuleChanged(const QnBusinessEventRulePtr &rule);
    void businessRuleDeleted(int id);
    void businessRuleReset(QnBusinessEventRuleList rules);

    void businessActionReceived(const QnAbstractBusinessActionPtr& action);

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);
public slots:
    void run();

private slots:
    void at_messageReceived(QnMessage message);
    void at_connectionClosed(QString errorString);
    void at_connectionOpened(QnMessage message);
    void at_serverIfFound(const QnMediaServerResourcePtr &resource, const QString & url, const QString& origApiUrl);

private:
    void init();
    void init(const QUrl& url, int reconnectTimeout);
    void determineOptimalIF(QnMediaServerResource* mediaServer);
    bool updateResource(QnResourcePtr resource, bool insert = true);
    void processResources(const QnResourceList& resources);
    void processLicenses(const QnLicenseList& licenses);
    void processCameraServerItems(const QnCameraHistoryList& cameraHistoryList);
    void updateHardwareIds(const QnMessage& message);

private:
    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    quint32 m_seqNumber;
    QSharedPointer<QnMessageSource> m_source;
};

#endif // _client_event_manager_h
