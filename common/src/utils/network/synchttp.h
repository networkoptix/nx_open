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
*/
class SyncHTTP : public QObject
{
    Q_OBJECT
    public:
        /// structors
        SyncHTTP (QObject * parent = 0)
            : requestID(0),
              status(false)
        {
        }

        SyncHTTP (const QHostAddress & hostName, quint16 port = 80, QAuthenticator auth = QAuthenticator(), QObject * parent = 0)
            : m_hostName(hostName),
              m_port(port),
              requestID(0),
              status(false)
        {
            m_credentials = auth.user() + ":" + auth.password();
            m_credentials = "Basic " + m_credentials.toAscii().toBase64();
        }

        virtual ~SyncHTTP ()
        {
        }

        /// send GET request and wait until finished
        int syncGet (const QString & path, QIODevice *to);

        /// send POST request and wait until finished
        int syncPost ( const QString & path, QIODevice * data, QIODevice * to );

        bool syncPost ( const QString & path, const QByteArray& data, QIODevice * to = 0 )
        {
            /// create io device from QByteArray
            QBuffer buffer;
            buffer.setData(data);
            return syncPost(path,&buffer,to);
        }

    protected slots:
        virtual void finished (QNetworkReply* reply)
        {
            /// check to see if it's the request we made
            if(reply != requestID)
                return;

            /// set status of the request
            status = reply->error();
            /// end the loop
            loop->exit();

            QObject::disconnect(this);
        }

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
