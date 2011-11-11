#include "synchttp.h"

int SyncHTTP::syncGet (const QString & path, QIODevice *to)
{
    // moveToThread(this);

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

    ///connect the requestFinished signal to our finished slot
    connect(m_httpClient.data(),SIGNAL(finished (QNetworkReply*)),SLOT(finished (QNetworkReply*)));
    /// start the request and store the requestID
    requestID = m_httpClient->get(request);
    /// block until the request is finished
    loop->exec();

    to->write(requestID->readAll());

    /// return the request status
    return status;
}

/// send POST request and wait until finished
int SyncHTTP::syncPost ( const QString & path, QIODevice * data, QIODevice * to )
{
//    moveToThread(QThread::currentThread());

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

    ///connect the requestFinished signal to our finished slot
    connect(m_httpClient.data(),SIGNAL(finished (QNetworkReply*)),SLOT(finished (QNetworkReply*)));
    /// start the request and store the requestID
    requestID = m_httpClient->post(request, data);
    /// block until the request is finished
    loop->exec();

    to->write(requestID->readAll());

    /// return the request status
    return status;
}
