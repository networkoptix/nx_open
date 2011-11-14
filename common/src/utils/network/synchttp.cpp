#include "synchttp.h"

SyncHTTP::SyncHTTP (const QHostAddress & hostName, quint16 port, QAuthenticator auth)
    : requestID(0),
      status(false),
      m_hostName(hostName),
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
    m_httpClient = QSharedPointer<QNetworkAccessManager>(new QNetworkAccessManager());
    loop = QSharedPointer<QEventLoop>(new QEventLoop());

    loop->moveToThread(QThread::currentThread());
    m_httpClient->moveToThread(QThread::currentThread());

    QUrl qurl;
    qurl.setScheme("http");
    qurl.setHost(m_hostName.toString());
    qurl.setPort(m_port);
    qurl.setPath(path);

    QNetworkRequest request(qurl);

    request.setRawHeader(QByteArray("Authorization"), m_credentials.toAscii());

    //connect the finished signal to our finished slot
    connect(m_httpClient.data(),SIGNAL(finished (QNetworkReply*)),SLOT(finished (QNetworkReply*)));

    // start the request and store the requestID
    requestID = m_httpClient->get(request);

    // block until the request is finished
    loop->exec();

    to->write(requestID->readAll());

    /// return the request status
    return status;
}

/// send POST request and wait until finished
int SyncHTTP::syncPost (const QString & path, QIODevice * data, QIODevice * to)
{
    m_httpClient = QSharedPointer<QNetworkAccessManager>(new QNetworkAccessManager());
    loop = QSharedPointer<QEventLoop>(new QEventLoop());

    loop->moveToThread(QThread::currentThread());
    m_httpClient->moveToThread(QThread::currentThread());

    QUrl qurl;
    qurl.setScheme("http");
    qurl.setHost(m_hostName.toString());
    qurl.setPort(m_port);
    qurl.setPath(path);

    QNetworkRequest request(qurl);

    request.setRawHeader(QByteArray("Authorization"),m_credentials.toAscii());

    // connect the finished signal to our finished slot
    connect(m_httpClient.data(),SIGNAL(finished (QNetworkReply*)),SLOT(finished (QNetworkReply*)));

    // start the request and store the requestID
    requestID = m_httpClient->post(request, data);

    // block until the request is finished
    loop->exec();

    to->write(requestID->readAll());

    // return the request status
    return status;
}

int SyncHTTP::syncPost (const QString& path, const QByteArray& data, QIODevice* to)
{
    /// create io device from QByteArray
    QBuffer buffer;
    buffer.setData(data);
    return syncPost(path,&buffer,to);
}

void SyncHTTP::finished (QNetworkReply* reply)
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
