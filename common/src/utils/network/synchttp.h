/***************************************************************************
 *   Copyright (C) 2005 by Iulian M                                        *
 *   eti@erata.net                                                         *
 ***************************************************************************/
#ifndef ETKSYNCHTTP_H
#define ETKSYNCHTTP_H

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QEventLoop>
#include <QBuffer>
#include <QAuthenticator>
#include <QNetworkRequest>
#include <QThread>

/**
 * Provide a synchronous api over QHttp
 * Uses a QEventLoop to block until the request is completed
 * @author Iulian M <eti@erata.net>
 * Improved by Ivan Vigasin <ivigasin@networkoptix.com>.
*/
class SyncHTTP : public QObject
{
Q_OBJECT
public:
    SyncHTTP (const QHostAddress & hostName, quint16 port = 80, QAuthenticator auth = QAuthenticator());
    virtual ~SyncHTTP ();

    // send GET request and wait until finished
    int syncGet (const QString & path, QIODevice *to);

    // send POST request and wait until finished
    int syncPost ( const QString & path, QIODevice * data, QIODevice * to );
    int syncPost ( const QString & path, const QByteArray& data, QIODevice * to = 0 );

protected slots:
    virtual void finished (QNetworkReply* reply);

private:
    QSharedPointer<QNetworkAccessManager> m_httpClient;

    /// id of current request
    QNetworkReply* requestID;

    /// error status of current request
    int status;

    /// event loop used to block until request finished
    QSharedPointer<QEventLoop> loop;

    QHostAddress m_hostName;
    quint16 m_port;
    QString m_credentials;
};

#endif
