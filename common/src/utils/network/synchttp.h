/***************************************************************************
 *   Copyright (C) 2005 by Iulian M                                        *
 *   eti@erata.net                                                         *
 ***************************************************************************/
#ifndef ETKSYNCHTTP_H
#define ETKSYNCHTTP_H

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkReply>

class QAuthenticator;
class QNetworkProxy;

/**
 * Provide a synchronous api over QHttp
 * Uses a QEventLoop to block until the request is completed
 *
 * @author Iulian M <eti@erata.net>
 * Improved by Ivan Vigasin <ivigasin@networkoptix.com>.
 */
class SyncHTTP : public QObject
{
    Q_OBJECT

public:
    SyncHTTP(const QUrl &url, QObject *parent = 0);
    virtual ~SyncHTTP();

    /**
     * Send GET request and wait until finished.
     */
    int syncGet(const QUrl &url, QIODevice *to = 0);

    /**
     * Send POST request and wait until finished.
     */
    int syncPost(const QUrl &url, QIODevice *data, QIODevice *to = 0);

    /**
     * Send POST request and wait until finished.
     */
    int syncPost(const QUrl &url, const QByteArray &data, QIODevice *to = 0);

    /**
     * Send asynchronous GET request. Given slot should take a pointer to QNetworkReply.
     */
    QNetworkReply *asyncGet(const QUrl &url, QObject *target, const char *slot);

    /**
     * Send asynchronous POST request. Given slot should take a pointer to QNetworkReply.
     */
    QNetworkReply *asyncPost(const QUrl &url, QIODevice *data, QObject *target, const char *slot);

    /**
     * Send asynchronous POST request. Given slot should take a pointer to QNetworkReply.
     */
    QNetworkReply *asyncPost(const QUrl &url, const QByteArray &data, QObject *target, const char *slot);

Q_SIGNALS:
    void error(int error);
    void authenticationRequired(QAuthenticator *authenticator);
#ifndef QT_NO_NETWORKPROXY
    void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator);
#endif

protected:
    enum Operation {
        GetOperation,
        PostOperation
    };

    QNetworkReply *asyncRequest(Operation op, const QUrl &url, QIODevice *data, QObject *target, const char *slot);
    int syncRequest(Operation op, const QUrl &url, QIODevice *data, QIODevice *outgoingData);

private Q_SLOTS:
    void onError(QNetworkReply::NetworkError error);
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);

private:
    QUrl m_url;
    QByteArray m_credentials;
};

#endif
