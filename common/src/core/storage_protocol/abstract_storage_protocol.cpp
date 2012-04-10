#include "abstract_storage_protocol.h"

QString QnAbstractStorageProtocol::removePrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QString QnAbstractStorageProtocol::getUrlPrefix() const
{
    return QString (getURLProtocol().name);
}

// -------------------------- QnStorageProtocolManager --------------------

Q_GLOBAL_STATIC(QnStorageProtocolManager, inst)

QnStorageProtocolManager* QnStorageProtocolManager::instance()
{
    return inst();
}

QnStorageProtocolManager::QnStorageProtocolManager()
{

}

QnStorageProtocolManager::~QnStorageProtocolManager()
{

}

QnAbstractStorageProtocolPtr QnStorageProtocolManager::getProtocol(const QString& url)
{
    QMutexLocker lock(&m_mutex);
    int protocol = url.indexOf("://");
    if (protocol == -1)
        return m_defaultProtocol;
    else
    {
        QnAbstractStorageProtocolPtr result = m_protocols.value(url.left(protocol));
        if (result)
            return result;
    }

    return m_defaultProtocol;
}

void QnStorageProtocolManager::registerProtocol(QnAbstractStorageProtocolPtr protocol, bool isDefaultProtocol)
{
    QMutexLocker lock(&m_mutex);
    m_protocols.insert(protocol->getURLProtocol().name, protocol);
    if (isDefaultProtocol)
        m_defaultProtocol = protocol;
}
