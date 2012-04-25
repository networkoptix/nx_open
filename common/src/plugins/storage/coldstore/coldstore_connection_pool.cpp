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
