#include "coldstore_connection_pool.h"

#ifdef ENABLE_COLDSTORE

#include "coldstore_storage.h"
#include "utils/common/sleep.h"


QnColdStoreConnection::QnColdStoreConnection(const QString& addr):
    m_opened(false),
    m_mutex(QMutex::Recursive)
{
    QMutexLocker lock(&m_mutex);

    QByteArray ipba = addr.toLatin1();
    char* ip = ipba.data();

    m_isConnected = false;

    m_isConnected = m_connection.Connect(ip) == Veracity::ISFS::STATUS_SUCCESS;
    //m_connection.EraseAll();
    
}

QnColdStoreConnection::~QnColdStoreConnection()
{
    QMutexLocker lock(&m_mutex);

    close();

    if (m_isConnected)
        m_connection.Disconnect();

}

bool QnColdStoreConnection::open(const QString& fn, QIODevice::OpenModeFlag flag, int channel)
{
    QMutexLocker lock(&m_mutex);

    if (!m_isConnected)
        return false;

    m_channel = channel;

    m_filename = fn;

    m_pos = 0;

    m_lastUsed.restart();
    m_openMode = flag;

    QByteArray baFn = fn.toLatin1();
    const char* cFn = baFn.data();
    Q_ASSERT(fn.length() < 64);


    if (m_openMode == QIODevice::ReadOnly)
    {

        Veracity::u32 status;

        while(1)
        {
            status = m_connection.Open(
                cFn, 
                0, //Timestamp of file to open. This is ignored unless (file_name == 0).
                channel, //Channel number of file to open.
                &m_stream, 
                0, //File name of the opened file. This is useful with a time based open.
                &m_openedStreamSize, //Size in bytes of the opened file.
                0,  //Current file position (measured in bytes from the start of the file). This is always 0 for a name based open.
                0 //Timestamp of the current position.
                );

            if (status != Veracity::ISFS::STATUS_DISK_POWERING_UP)
                break;

            QnSleep::msleep(50); // to avoid net and cpu usage

        }

        if ( status != Veracity::ISFS::STATUS_SUCCESS)
        {
            //Q_ASSERT(false);
            return false;
        }
    }
    else if (m_openMode == QIODevice::WriteOnly)
    {
        if (m_connection.Create(
            cFn,
            0, //Channel number of file to create
            &m_stream,
            0 //Current system time on the COLDSTORE unit.
            ) != Veracity::ISFS::STATUS_SUCCESS)
        {
            Q_ASSERT(false);
            return false;
        }
    }
    else
    {
        Q_ASSERT(false);
        return false;
    }

    m_opened = true;
    return true;
}

qint64 QnColdStoreConnection::size() const
{
    QMutexLocker lock(&m_mutex);
    return m_openedStreamSize;
}

void QnColdStoreConnection::close()
{
    QMutexLocker lock(&m_mutex);
    if (!m_isConnected || !m_opened)
        return;

    m_opened = false;

    m_lastUsed.restart();

    /*Veracity::u32 status =*/
    m_connection.Close(m_stream);

    //qDebug() << " cs file closed==================!!!!:" << m_filename << " channel " << m_channel;

}


bool QnColdStoreConnection::seek(qint64 pos)
{
    QMutexLocker lock(&m_mutex);
    if (!m_isConnected || !m_opened)
        return false;

    

    m_lastUsed.restart();

    if (m_pos == pos)
        return true; // does not need to seek extra time 

    Veracity::u32 status = 
        m_connection.Seek(
        m_stream, 
        pos, 
        0, //0 – Do step 1 (file_offset is relative to beginning of file).
        0, //If (seek_whence != 0xff) then this parameter is ignored.
        0, //If (seek_direction == 0xff) then this parameter is ignored
        0xff //seek_direction 0xff – Skip step 2
        );

    if (status != Veracity::ISFS::STATUS_SUCCESS)
    {
        Q_ASSERT(false);
        return false;
    }

    m_pos = pos;


    //qWarning() << "SEEK operation for " << m_filename <<  "  pos=" << pos;
    return true;
}

bool QnColdStoreConnection::write(const char* data, int len)
{
    QMutexLocker lock(&m_mutex);
    if (!m_isConnected || !m_opened)
        return false;

    m_lastUsed.restart();

    Veracity::u64 returned_data_length;

    

    if (m_connection.Write(
        m_stream,
        data,
        len,
        0,//data_tag
        0, //Timestamp of the chunk.
        &returned_data_length
        )  != Veracity::ISFS::STATUS_SUCCESS )
    {
        //Q_ASSERT(false);
        return false;
    }

    m_pos += returned_data_length;

    Q_ASSERT(returned_data_length == (Veracity::u64)len);

    return true;
}


int QnColdStoreConnection::read(char *data, int len)
{
    QMutexLocker lock(&m_mutex);
    if (!m_isConnected || !m_opened)
        return -1;

    

    m_lastUsed.restart();

    Veracity::u64 readed;
    

    if (m_connection.Read(
        m_stream,
        (void*)data,
        len,
        len,
        &readed,
        0 //returned_data_remaining
        ) != Veracity::ISFS::STATUS_SUCCESS)
    {
        Q_ASSERT(false);
        return -1;
    }

    m_pos += readed;

    //qWarning() << "READ operation for " << m_filename <<  "  curr_pos=" << m_pos << " readLen=" << len;

    return readed;
}

qint64 QnColdStoreConnection::oldestFileTime(const QString& fn)
{
    QMutexLocker lock(&m_mutex);
    m_lastUsed.restart();

    QByteArray baFn = fn.toLatin1();
    char* cFn = baFn.data();

    Veracity::u32 resultSize;
    Veracity::u32 resultCount;
    char * result = new char[1024*1204];

    m_connection.FileQuery
        (
        false, //False = duplicate results allowed in the result set
        false, //False = don’t return the channel number in the result set
        false, //false = don’t return the filename in the result set
        true,//True = return the first_ms in the result set
        0,
        0,
        cFn,
        0, //The channel number to match,
        true, //True – Use the channel in the file query
        1, //limit resluts 
        false,
        0,
        result,
        &resultSize,
        &resultCount);

    if (resultCount==0)
        return 0;

    qint64 resultT = QString(QLatin1String(result)).toULongLong();

    delete result;

    return resultT;
}

int QnColdStoreConnection::age() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastUsed.elapsed()/1000;
}

QString QnColdStoreConnection::getFilename() const
{
    QMutexLocker lock(&m_mutex);
    return m_filename;
}

qint64 QnColdStoreConnection::pos() const
{
    QMutexLocker lock(&m_mutex);
    return m_pos;
}

void QnColdStoreConnection::externalLock() const
{
    m_mutex.lock();
}

void QnColdStoreConnection::externalUnLock() const
{
    m_mutex.unlock();
}


//==================================================================================

QnColdStoreConnectionPool::QnColdStoreConnectionPool(QnPlColdStoreStorage *csStorage):
m_csStorage(csStorage),
m_mutex(QMutex::Recursive)
{
    
#ifdef Q_OS_WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(1, 1);
    WSAStartup(wVersionRequested, &wsaData);
#endif
}

QnColdStoreConnectionPool::~QnColdStoreConnectionPool()
{
    QMutexLocker lock(&m_mutex);

    qDeleteAll(m_pool);
    m_pool.clear();
}

int QnColdStoreConnectionPool::read(const QString& csFn,   char* data, quint64 shift, int len)
{
    QMutexLocker lock(&m_mutex);

    checkIfSomeConnectionsNeedToBeClosed();

    QHash<QString, QnColdStoreConnection*>::iterator it = m_pool.find(csFn);

    QnColdStoreConnection* connection;

    if (it == m_pool.end())
    {
        connection = new QnColdStoreConnection(m_csStorage->coldstoreAddr());
        if (connection->open(csFn, QIODevice::ReadOnly, CS_ACTUAL_DATA_CHANNEL))
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

    m_mutex.unlock();
    
    connection->externalLock();
    if (!connection->seek(shift))
    {
        connection->externalUnLock();
        m_mutex.lock();

        return -1;
    }
    int result = connection->read(data, len);
    connection->externalUnLock();

    m_mutex.lock();

    
    return result;
}


void QnColdStoreConnectionPool::checkIfSomeConnectionsNeedToBeClosed()
{
    QMutexLocker lock(&m_mutex);

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

#endif // ENABLE_COLDSTORE

