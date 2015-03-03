#ifndef DIRECT_MODULE_FINDER_H
#define DIRECT_MODULE_FINDER_H

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QElapsedTimer>

class QTimer;
class QnAsyncHttpClientReply;
struct QnModuleInformationEx;

class QnDirectModuleFinder : public QObject {
    Q_OBJECT
public:
    explicit QnDirectModuleFinder(QObject *parent = 0);

    void setCompatibilityMode(bool compatibilityMode);
    bool isCompatibilityMode() const;

    void addUrl(const QUrl &url);
    void removeUrl(const QUrl &url);
    void checkUrl(const QUrl &url);
    void setUrls(const QSet<QUrl> &urls);

    QSet<QUrl> urls() const;

    void start();
    void pleaseStop();

signals:
    void responseRecieved(const QnModuleInformationEx &moduleInformation, const QUrl &url);

private:
    void enqueRequest(const QUrl &url);

private slots:
    void activateRequests();

    void at_reply_finished(QnAsyncHttpClientReply *reply);
    void at_checkTimer_timeout();

private:
    bool m_compatibilityMode;

    QSet<QUrl> m_urls;
    int m_maxConnections;
    QQueue<QUrl> m_requestQueue;
    QMap<QUrl, QnAsyncHttpClientReply*> m_activeRequests;
    QHash<QUrl, qint64> m_lastPingByUrl;
    QHash<QUrl, qint64> m_lastCheckByUrl;

    QTimer *m_checkTimer;
    QElapsedTimer m_elapsedTimer;
};

#endif // DIRECT_MODULE_FINDER_H
