/***************************************************************************
 *   Copyright (C) 2005 by Iulian M                                        *
 *   eti@erata.net                                                         *
 ***************************************************************************/
#ifndef ETKSYNCHTTP_H
#define ETKSYNCHTTP_H

#include <QAuthenticator>
#include <QHostAddress>

class QIODevice;
class QByteArray;

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
    SyncHTTP(const QHostAddress &hostName, quint16 port = 80, const QAuthenticator &auth = QAuthenticator());
    virtual ~SyncHTTP();

    /**
     * Send GET request and wait until finished.
     */
    int syncGet(const QString &path, QIODevice *to);

    /**
     * Send POST request and wait until finished. 
     */
    int syncPost(const QString &path, QIODevice *data, QIODevice *to);

    /**
     * Send POST request and wait until finished. 
     */
    int syncPost(const QString &path, const QByteArray &data, QIODevice *to);

    /**
     * Send asynchronous GET request. Given slot should take a pointer to QNetworkReply.
     */
    void asyncGet(const QString &path, QObject *target, const char *slot);

    /**
     * Send asynchronous POST request. Given slot should take a pointer to QNetworkReply.
     */
    void asyncPost(const QString &path, QIODevice *data, QObject *target, const char *slot);

    /**
     * Send asynchronous POST request. Given slot should take a pointer to QNetworkReply.
     */
    void asyncPost(const QString &path, const QByteArray &data, QObject *target, const char *slot);

protected:
    template<class Sender>
    int syncRequest(const Sender &sender, const QString &path, QIODevice *data, QIODevice *to);

    template<class Sender>
    void asyncRequest(const Sender &sender, const QString &path, QIODevice *data, QObject *target, const char *slot);

private:
    QHostAddress m_hostName;
    quint16 m_port;
    QString m_credentials;
};

#endif
