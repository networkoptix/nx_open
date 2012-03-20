#ifndef _client_event_manager_h
#define _client_event_manager_h

#include <QSharedPointer>

#include "api/EventSource.h"
#include "api/AppServerConnection.h"
#include "core/resource/resource.h"

class QnEventManager : public QObject
{
    Q_OBJECT

public:
    static QnEventManager *instance();

    QnEventManager();

    void stop();

signals:
    void connectionOpened();
    void connectionClosed();

public slots:
    void run();

private slots:
    void resourcesReceived(int status, const QByteArray& errorString, QnResourceList resources, int handle);
    void licensesReceived(int status, const QByteArray& errorString, QnLicenseList licenses, int handle);

    void eventReceived(QnEvent event);
    void connectionClosed(QString errorString);

private:
    void init();
    void init(const QUrl& url, int reconnectTimeout);

private:
    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    QSharedPointer<QnEventSource> m_source;
};

#endif // _client_event_manager_h
