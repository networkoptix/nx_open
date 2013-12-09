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

/*
    //client
    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));

    //server
    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));

    connect(this, SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)), qnBusinessRuleProcessor, SLOT(at_businessRuleChanged(QnBusinessEventRulePtr)));
    connect(this, SIGNAL(businessRuleDeleted(int)), qnBusinessRuleProcessor, SLOT(at_businessRuleDeleted(int)));
    connect(this, SIGNAL(businessRuleReset(QnBusinessEventRuleList)), qnBusinessRuleProcessor, SLOT(at_businessRuleReset(QnBusinessEventRuleList)));


    // proxy

    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));

    connect(this, SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)), qnBusinessRuleProcessor, SLOT(at_businessRuleChanged(QnBusinessEventRulePtr)));
    connect(this, SIGNAL(businessRuleDeleted(int)), qnBusinessRuleProcessor, SLOT(at_businessRuleDeleted(int)));*/
}
