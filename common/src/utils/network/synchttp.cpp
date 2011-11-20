#include "synchttp.h"

SyncHTTP::SyncHTTP (const QHostAddress & hostName, quint16 port, QAuthenticator auth)
    : m_hostName(hostName),
      m_port(port)
{
    m_credentials = auth.user() + ":" + auth.password();
    m_credentials = "Basic " + m_credentials.toAscii().toBase64();
}

SyncHTTP::~SyncHTTP ()
{
}

int SyncHTTP::syncGet (const QString& path, QIODevice* to)
{
    QNetworkAccessManager accessManager;
    QEventLoop loop;

    QUrl qurl("http://" + m_hostName.toString() + ":" + QString::number(m_port) + "/" + path);

    QNetworkRequest request(qurl);

    request.setRawHeader(QByteArray("Authorization"), m_credentials.toAscii());

    connect(&accessManager, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));

    // start the request 
    QScopedPointer<QNetworkReply> reply(accessManager.get(request));

    // block until the request is finished
    loop.exec();

    // set status of the request
    int error = reply->error();

    to->write(reply->readAll());

    // return the request status
    return error;
}

/// send POST request and wait until finished
int SyncHTTP::syncPost (const QString & path, QIODevice * data, QIODevice * to)
{
    QNetworkAccessManager accessManager;
    QEventLoop loop;

    QUrl qurl("http://" + m_hostName.toString() + ":" + QString::number(m_port) + "/" + path);

    QNetworkRequest request(qurl);

    request.setRawHeader(QByteArray("Authorization"), m_credentials.toAscii());

    connect(&accessManager, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));

    // start the request 
    QScopedPointer<QNetworkReply> reply(accessManager.post(request, data));

    // block until the request is finished
    loop.exec();

    // set status of the request
    int error = reply->error();

    to->write(reply->readAll());

    // return the request status
    return error;
}

int SyncHTTP::syncPost (const QString& path, const QByteArray& data, QIODevice* to)
{
    /// create io device from QByteArray
    QBuffer buffer;
    buffer.setData(data);
    return syncPost(path,&buffer,to);
}

