#include "coldstore_connection_pool.h"


QnColdStoreConnection::QnColdStoreConnection(const QString& addr)
{
    QByteArray ipba = addr.toLocal8Bit();
    char* ip = ipba.data();

    m_isConnected = false;

    m_isConnected = m_connection.Connect(ip) == Veracity::ISFS::STATUS_SUCCESS;
}

QnColdStoreConnection::~QnColdStoreConnection()
{

}

bool QnColdStoreConnection::open(const QString& fn, QIODevice::OpenModeFlag flag)
{
    return true;
}

void QnColdStoreConnection::close()
{

}


bool QnColdStoreConnection::seek(qint64 pos)
{
    return true;
}

bool QnColdStoreConnection::write(const char* data, int len)
{
    return true;
}

int QnColdStoreConnection::read(char *data, int len)
{
    return 0;
}

int QnColdStoreConnection::age() const
{
    return 0;
}
//==================================================================================

QnColdStoreConnectionPool::QnColdStoreConnectionPool(const QString& addr):
m_csAddr(addr)
{

}

QnColdStoreConnectionPool::~QnColdStoreConnectionPool()
{
    QHashIterator<QString, QnColdStoreConnection*> it(m_pool);
    while (it.hasNext()) 
    {
        it.next();
        delete it.value();
    }
}

int QnColdStoreConnectionPool::read(const QString& csFn,   char* data, quint64 shift, int len)
{
    QHash<QString, QnColdStoreConnection*>::iterator it = m_pool.find(csFn);

    QnColdStoreConnection* connection;

    if (it == m_pool.end())
    {
        connection = new QnColdStoreConnection(m_csAddr);
        if (connection->open(csFn, QIODevice::ReadOnly))
        {
            m_pool.insert(csFn, connection);
        }
        else
        {
            delete connection;
            return -1;
        }
    }
    else
    {
        connection = it.value();
    }

    
    connection->seek(shift);

    int result = connection->read(data, len);
    checkIfSomeConnectionsNeedToBeClosed();
    return result;
}


void QnColdStoreConnectionPool::checkIfSomeConnectionsNeedToBeClosed()
{

    if (m_pool.size() <= 20)
        return;

    QHash<QString, QnColdStoreConnection*>::iterator it = m_pool.begin();
    while (it != m_pool.end()) 
    {
        if (it.value()->age() > 20) // if last request to this connection was made more than 20 sec ago 
        {
            delete it.value();
            it = m_pool.erase(it);
        }
        else
            ++it;
    }
    
}