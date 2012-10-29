#include "business_message_bus.h"

QnBusinessMessageBus::QnBusinessMessageBus()
{
    connect(&m_transport, SIGNAL(finished(QNetworkReply*)), this, SLOT(at_replyFinished(QNetworkReply*)));
}

/*
int QnBusinessMessageBus::deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url)
{
    return 0;
}
*/

int QnBusinessMessageBus::deliveryBusinessAction(QnAbstractBusinessActionPtr bAction, const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/data"));
    QNetworkReply* reply = m_transport.post(request, bAction->serialize());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_replyError(QNetworkReply::NetworkError)));

    return 0;
}

void QnBusinessMessageBus::at_replyFinished(QNetworkReply* reply)
{
    QnAbstractBusinessActionPtr action;
    {
        QMutexLocker lock(&m_mutex);
        ActionMap::Iterator itr =  m_actionsInProgress.find(reply);
        Q_ASSERT(itr != m_actionsInProgress.end());
        action = itr.value();
        m_actionsInProgress.erase(itr);
        reply->deleteLater();
    }
    emit actionDelivered(action);
}

void QnBusinessMessageBus::at_replyError(QNetworkReply::NetworkError code)
{
    QnAbstractBusinessActionPtr action;
    QNetworkReply* reply = (QNetworkReply*) sender();
    {
        QMutexLocker lock(&m_mutex);
        ActionMap::Iterator itr =  m_actionsInProgress.find(reply);
        Q_ASSERT(itr != m_actionsInProgress.end());
        action = itr.value();
        m_actionsInProgress.erase(itr);
        reply->deleteLater();
    }
    emit actionDeliveryFail(action);
}

void QnBusinessMessageBus::at_actionReceived(QnAbstractBusinessActionPtr action)
{
    emit actionReceived(action);
}
