/**********************************************************
* 18 apr 2013
* akolesnikov
***********************************************************/

#include "sync_http_client_delegate.h"

#include <QMutexLocker>
#include <QThread>

#include "sync_http_client.h"


SyncHttpClientDelegate::SyncHttpClientDelegate( QNetworkAccessManager* networkAccessManager )
:
    m_networkAccessManager( networkAccessManager ),
    m_reply( NULL ),
    m_requestState( rsInit )
{
    connect( m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onConnectionFinished(QNetworkReply*)) );
}

SyncHttpClientDelegate::~SyncHttpClientDelegate()
{
    disconnect( m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onConnectionFinished(QNetworkReply*)) );
}

void SyncHttpClientDelegate::get( const QNetworkRequest& request )
{
    Q_ASSERT( QThread::currentThread() != thread() );
 
    QMutexLocker lk( &m_mutex );

    if( m_reply )
    {
        m_reply->deleteLater();
        m_reply = NULL;
    }

    m_requestState = rsInit;

    QMetaObject::invokeMethod(
        this,
        "getPriv",
        Qt::QueuedConnection,
        Q_ARG(QNetworkRequest, request) );

    while( m_requestState <= rsWaitingResponse )
        m_cond.wait( lk.mutex() );
}

QByteArray SyncHttpClientDelegate::readWholeMessageBody()
{
    Q_ASSERT( QThread::currentThread() != thread() );

    if( m_requestState > rsReadingMessageBody )
        return QByteArray();

    QMetaObject::invokeMethod( this, "readWholeMessageBodyPriv", Qt::QueuedConnection );

    QMutexLocker lk( &m_mutex );
    while( m_requestState <= rsReadingMessageBody )
        m_cond.wait( lk.mutex() );
    QByteArray msgBody = m_messageBody;
    m_messageBody.clear();
    return msgBody;
}

QNetworkReply::NetworkError SyncHttpClientDelegate::resultCode() const
{
    QMutexLocker lk( &m_mutex );
    return m_reply ? m_reply->error() : QNetworkReply::NoError;
}

int SyncHttpClientDelegate::statusCode() const
{
    QMutexLocker lk( &m_mutex );
    return m_reply
        ? m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
        : SyncHttpClient::HTTP_OK;
}

void SyncHttpClientDelegate::getPriv( const QNetworkRequest& request )
{
    QMutexLocker lk( &m_mutex );

    QUrl reqUrl = request.url();

    if( m_reply )
        m_reply->deleteLater();
    m_reply = m_networkAccessManager->get( request );
    connect( m_reply, SIGNAL(readyRead()), this, SLOT(onReplyReadyRead()) );

    m_requestState = rsWaitingResponse;
}

void SyncHttpClientDelegate::onReplyReadyRead()
{
    QMutexLocker lk( &m_mutex );

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    if( reply != m_reply )
        return; //this can be signal from old reply (being deleted)

    if( m_requestState <= rsWaitingResponse )
    {
        m_requestState = rsReadingMessageBody;
        m_cond.wakeAll();
    }
}

void SyncHttpClientDelegate::onConnectionFinished( QNetworkReply* reply )
{
    QMutexLocker lk( &m_mutex );

    if( reply != m_reply )
        return; //this can be signal from old reply (being deleted)

    m_requestState = rsDone;
    m_cond.wakeAll();
}

void SyncHttpClientDelegate::readWholeMessageBodyPriv()
{
    QMutexLocker lk( &m_mutex );
    m_messageBody = m_reply->readAll();
    m_requestState = rsDone;
    m_cond.wakeAll();
}
