#include "business_message_bus.h"

Q_GLOBAL_STATIC(QnBusinessMessageBus, inst)

QnBusinessMessageBus* QnBusinessMessageBus::instance()
{
    QnBusinessMessageBus* _instance = inst();
    if( !_instance->isRunning() )
        _instance->start();
    return _instance;
}

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

    QByteArray data;
    m_serializer.serializeBusinessAction(bAction, data);
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

void QnBusinessMessageBus::at_actionReceived(QnAbstractBusinessActionPtr action)
{
    emit actionReceived(action);
}
