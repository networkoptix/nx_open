#include "business_message_bus.h"

#include <QtCore/QUrlQuery>

#include "../../appserver2/src/transaction/transaction.h"

Q_GLOBAL_STATIC(QnBusinessMessageBus, QnBusinessMessageBus_instance)

QnBusinessMessageBus* QnBusinessMessageBus::instance()
{
    QnBusinessMessageBus* _instance = QnBusinessMessageBus_instance();
    if( !_instance->isRunning() )
        _instance->start();
    return _instance;
}

QnBusinessMessageBus::QnBusinessMessageBus()
{
    connect(&m_transport, SIGNAL(finished(QNetworkReply*)), this, SLOT(at_replyFinished(QNetworkReply*)));
}

QnBusinessMessageBus::~QnBusinessMessageBus()
{
    quit();
    wait();
}

/*
int QnBusinessMessageBus::deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url)
{
    return 0;
}
*/

int QnBusinessMessageBus::deliveryBusinessAction(const QnAbstractBusinessActionPtr &bAction, const QnResourcePtr &res, const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    QNetworkRequest request;

    QUrl u(url);
    if (res)
    {
        QUrlQuery urlQuery(u.query());
        urlQuery.addQueryItem(QLatin1String("resource"), res->getId().toString()); // execute action for 1 resource only
        u.setQuery(urlQuery);
    }

    request.setUrl(u);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/data"));

    QByteArray data;
    OutputBinaryStream<QByteArray> stream(&data);
    QnBinary::serialize(bAction, &stream);
    QNetworkReply* reply = m_transport.post(request, data);
    m_actionsInProgress.insert(reply, bAction);

    return 0;
}

void QnBusinessMessageBus::at_replyFinished(QNetworkReply* reply)
{
    QnAbstractBusinessActionPtr action;
    QNetworkReply::NetworkError replyResultCode = QNetworkReply::NoError;
    {
        QMutexLocker lock(&m_mutex);
        ActionMap::Iterator itr =  m_actionsInProgress.find(reply);
        Q_ASSERT(itr != m_actionsInProgress.end());
        action = itr.value();
        m_actionsInProgress.erase(itr);
        replyResultCode = reply->error();
        reply->deleteLater();
    }
    if( replyResultCode == QNetworkReply::NoError )
        emit actionDelivered(action);
    else
        emit actionDeliveryFail(action);
}

void QnBusinessMessageBus::at_actionReceived(const QnAbstractBusinessActionPtr &action, const QnResourcePtr &res)
{
    emit actionReceived(action, res);
}
