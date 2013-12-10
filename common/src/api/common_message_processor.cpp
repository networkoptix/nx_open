#include "common_message_processor.h"

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    QObject(parent)
{
}

void QnCommonMessageProcessor::run() {
    m_source->startRequest();
}

void QnCommonMessageProcessor::stop() {
    if (m_source)
        m_source->stop();
}

void QnCommonMessageProcessor::init(const QUrl &url, const QString &authKey, int reconnectTimeout) {
    m_source = QSharedPointer<QnMessageSource>(new QnMessageSource(url, reconnectTimeout));
    m_source->setAuthKey(authKey);

    connect(m_source.data(), SIGNAL(connectionReset()), this, SIGNAL(connectionReset()));

/*
    //client
    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));

    //server
    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));

    // proxy

    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
*/
}

void QnCommonMessageProcessor::handleMessage(const QnMessage &message) {
    switch(message.messageType) {
    case Qn::Message_Type_Ping:
        {
            break;
        }
    case Qn::Message_Type_BusinessRuleInsertOrUpdate:
        {
            emit businessRuleChanged(message.businessRule);
            break;
        }
    case Qn::Message_Type_BusinessRuleReset:
        {
            emit businessRuleReset(message.businessRules);
            break;
        }
    case Qn::Message_Type_BusinessRuleDelete:
        {
            emit businessRuleDeleted(message.resourceId.toInt());
            break;
        }
    case Qn::Message_Type_BroadcastBusinessAction:
        {
            emit businessActionReceived(message.businessAction);
            break;
        }
    default:
        break;
    }
}
