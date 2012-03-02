#ifndef _server_event_manager_h
#define _server_event_manager_h

#include <QSharedPointer>

#include "api/EventSource.h"
#include "core/resource/resource.h"

class QnEventManager : public QObject
{
    Q_OBJECT

public:
    static QnEventManager* instance();

    QnEventManager();

    void init(const QUrl& url, int reconnectTimeout);
    void stop();

public slots:
	void resourcesReceived(int status, const QByteArray& errorString, QnResourceList resources, int handle);
    void run();

private slots:
    void eventReceived(QnEvent event);
    void connectionClosed(QString errorString);

private:
    QSharedPointer<QnEventSource> m_source;
};

#endif // _server_event_manager_h
