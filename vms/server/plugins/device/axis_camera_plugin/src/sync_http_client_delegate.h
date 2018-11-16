/**********************************************************
* 18 apr 2013
* akolesnikov
***********************************************************/

#ifndef SYNC_HTTP_CLIENT_DELEGATE_H
#define SYNC_HTTP_CLIENT_DELEGATE_H

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QWaitCondition>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>


//!Helper class, used by SyncHttpClient
/*!
    This class instance works in \a QNetworkAccessManager thread. This is needed to allow \a SyncHttpClient to belong to any thread
*/
class SyncHttpClientDelegate
:
    public QObject
{
    Q_OBJECT

public:
    SyncHttpClientDelegate( QNetworkAccessManager* networkAccessManager );

    //!Blocks until current request has completed (valid response has been received or error occured)
    void get( const QNetworkRequest& request );

    //!Blocks till all message body read
    /*!
        \note Can be called from any thread
    */
    QByteArray readWholeMessageBody();

    QNetworkReply::NetworkError resultCode() const;
    int statusCode() const;

protected:
    virtual ~SyncHttpClientDelegate();

private:
    enum RequestState
    {
        rsInit,
        rsWaitingResponse,
        rsReadingMessageBody,
        rsDone,
        rsFailed
    };

    QNetworkAccessManager* m_networkAccessManager;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    QNetworkReply* m_reply;
    QByteArray m_messageBody;
    RequestState m_requestState;

private slots:
    /*!
        \note Can be called from object's thread only
    */
    void getPriv( const QNetworkRequest& request );
    void onReplyReadyRead();
    void onConnectionFinished( QNetworkReply* reply );
    //!Reads reply message body to \a m_messageBody
    void readWholeMessageBodyPriv();
};

#endif  //SYNC_HTTP_CLIENT_DELEGATE_H
