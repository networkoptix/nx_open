#ifndef DIRECT_MODULE_FINDER_H
#define DIRECT_MODULE_FINDER_H

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QElapsedTimer>

class QTimer;
class QnAsyncHttpClientReply;
class HostAddress;
class SocketAddress;
struct QnModuleInformation;

class QnDirectModuleFinder : public QObject {
    Q_OBJECT
public:
    explicit QnDirectModuleFinder(QObject *parent);

    void addUrl(const QUrl &url);
    void removeUrl(const QUrl &url);
    void setUrls(const QSet<QUrl> &urls);

    QSet<QUrl> urls() const;

    void start();
    void pleaseStop();

public slots:
    void checkUrl(const QUrl &url);

signals:
    void responseReceived(
        const QnModuleInformation &moduleInformation,
        const SocketAddress &endpoint, const HostAddress &ip);

private:
    void enqueRequest(const QUrl &url);

private slots:
    void activateRequests();

    void at_reply_finished(QnAsyncHttpClientReply *reply);
    void at_checkTimer_timeout();

private:
    QSet<QUrl> m_urls;
    const int m_maxConnections;
    QQueue<QUrl> m_requestQueue;
    QMap<QUrl, QnAsyncHttpClientReply*> m_activeRequests;
    QHash<QUrl, qint64> m_lastPingByUrl;
    QHash<QUrl, qint64> m_lastCheckByUrl;

    QTimer *m_checkTimer;
    QElapsedTimer m_elapsedTimer;

    std::chrono::milliseconds maxPingTimeout() const;
    std::chrono::milliseconds aliveCheckInterval() const;
    std::chrono::milliseconds discoveryCheckInterval() const;
};

#endif // DIRECT_MODULE_FINDER_H
