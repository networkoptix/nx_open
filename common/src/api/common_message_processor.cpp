#include "common_message_processor.h"

#include <api/message_source.h>
#include <api/app_server_connection.h>

#include <business/business_event_rule.h>

#include <core/resource_management/resource_pool.h>

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    QObject(parent)
{
}

void QnCommonMessageProcessor::run() {
    m_source = QSharedPointer<QnMessageSource>(new QnMessageSource(m_url, m_reconnectTimeout));
    m_source->setAuthKey(m_authKey);
    m_source->setVideoWallKey(m_videoWallKey);

    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()),          this, SIGNAL(connectionReset()));
    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));

    m_source->startRequest();
}

void QnCommonMessageProcessor::stop() {
    if (m_source) {
        m_source->stop();
    }
}

void QnCommonMessageProcessor::init(const QUrl &url, const QString &authKey, int reconnectTimeout) {
    m_url = url;
    m_authKey = authKey;
    m_reconnectTimeout = reconnectTimeout;
}

void QnCommonMessageProcessor::loadRuntimeInfo(const QnMessage &message) {
    if (!message.systemName.isEmpty())
        QnAppServerConnectionFactory::setSystemName(message.systemName);
    if (!message.publicIp.isEmpty())
        QnAppServerConnectionFactory::setPublicIp(message.publicIp);
    if (!message.sessionKey.isEmpty())
        QnAppServerConnectionFactory::setSessionKey(message.sessionKey);
}

void QnCommonMessageProcessor::handleConnectionOpened(const QnMessage &message) {
    // TODO: #Elric this is a hack to update system name before emitting connectionOpened
    // as it is used in handers.

    if(message.messageType == Qn::Message_Type_Initial)
        loadRuntimeInfo(message); 

    emit connectionOpened();
}

void QnCommonMessageProcessor::handleConnectionClosed(const QString &errorString) {
    qDebug() << "Connection aborted:" << errorString;
    emit connectionClosed();
}

void QnCommonMessageProcessor::handleMessage(const QnMessage &message) {
    switch(message.messageType) {
    case Qn::Message_Type_Initial: {
        loadRuntimeInfo(message);
        updateKvPairs(message.kvPairs);
        break;
    }
    case Qn::Message_Type_Ping: {
        break;
    }
    case Qn::Message_Type_RuntimeInfoChange: {
        loadRuntimeInfo(message);
        break;
    }
    case Qn::Message_Type_BusinessRuleInsertOrUpdate: {
        emit businessRuleChanged(message.businessRule);
        break;
    }
    case Qn::Message_Type_BusinessRuleReset: {
        emit businessRuleReset(message.businessRules);
        break;
    }
    case Qn::Message_Type_BusinessRuleDelete: {
        emit businessRuleDeleted(message.resourceId.toInt());
        break;
    }
    case Qn::Message_Type_BroadcastBusinessAction: {
        emit businessActionReceived(message.businessAction);
        break;
    }
    case Qn::Message_Type_FileAdd: {
        emit fileAdded(message.filename);
        break;
    }
    case Qn::Message_Type_FileRemove: {
        emit fileRemoved(message.filename);
        break;
    }
    case Qn::Message_Type_FileUpdate: {
        emit fileUpdated(message.filename);
        break;
    }
    case Qn::Message_Type_KvPairChange: {
        updateKvPairs(message.kvPairs);
        break;
    }
    case Qn::Message_Type_KvPairDelete: {
        foreach (int resourceId, message.kvPairs.keys()) {
            QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::AllResources);
            if (!resource)
                continue;
            QnKvPairList values = message.kvPairs[resourceId];
            foreach (const QnKvPair &pair, values)
                resource->setProperty(pair.name(), QString());
        }
        break;
    }
    default:
        break;
    }
}

void QnCommonMessageProcessor::updateKvPairs(const QnKvPairListsById &kvPairs) {
    foreach (int resourceId, kvPairs.keys()) {
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::AllResources);
        if (!resource)
            continue;
        QnKvPairList values = kvPairs[resourceId];
        foreach (const QnKvPair &pair, values)
            resource->setProperty(pair.name(), pair.value());
    }
}

void QnCommonMessageProcessor::at_connectionOpened(const QnMessage &message) {
    handleConnectionOpened(message);
}

void QnCommonMessageProcessor::at_connectionClosed(const QString &errorString) {
    handleConnectionClosed(errorString);
}

void QnCommonMessageProcessor::at_messageReceived(const QnMessage &message) {
    handleMessage(message);
}
