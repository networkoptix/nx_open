#ifndef _server_event_manager_h
#define _server_event_manager_h

#include <QSharedPointer>

#include "api/EventSource.h"

class QnEventManager : public QObject
{
    Q_OBJECT

public:
    static QnEventManager* instance();

    QnEventManager();

    void init(const QUrl& url, int reconnectTimeout);
    void stop();

signals:
    void connectionReset();

public slots:
    void run();

private slots:
    void at_eventReceived(QnEvent event);
    void at_connectionClosed(QString errorString);
    void at_connectionReset();
private:
    QSharedPointer<QnEventSource> m_source;
};

#endif // _server_event_manager_h
