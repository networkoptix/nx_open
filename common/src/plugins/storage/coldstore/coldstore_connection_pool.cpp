#include "coldstore_connection_pool.h"
#include "coldstore_storage.h"


QnColdStoreConnection::QnColdStoreConnection(const QString& addr)
{
    QByteArray ipba = addr.toLocal8Bit();
    char* ip = ipba.data();

    m_isConnected = false;

    m_isConnected = m_connection.Connect(ip) == Veracity::ISFS::STATUS_SUCCESS;
}

QnColdStoreConnection::~QnColdStoreConnection()
{
    close();

    if (m_isConnected)
        m_connection.Disconnect();

}

bool QnColdStoreConnection::open(const QString& fn, QIODevice::OpenModeFlag flag, int channel)
{
    if (!m_isConnected)
        return false;

    m_filename = fn;

    m_pos = 0;

    m_lastUsed.restart();
    m_openMode = flag;

    QByteArray baFn = fn.toLatin1();
    const char* cFn = baFn.data();
    Q_ASSERT(fn.length() < 64);

    if (m_openMode == QIODevice::ReadOnly)
    {
        Veracity::u32 status = m_connection.Open(
            cFn, 
            0, //Timestamp of file to open. This is ignored unless (file_name == 0).
            channel, //Channel number of file to open.
            &m_stream, 
            0, //File name of the opened file. This is useful with a time based open.
            &m_openedStreamSize, //Size in bytes of the opened file.
            0,  //Current file position (measured in bytes from the start of the file). This is always 0 for a name based open.
            0 //Timestamp of the current position.
            );

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


    return true;
}

qint64 QnColdStoreConnection::size() const
{
    return m_openedStreamSize;
}

void QnColdStoreConnection::close()
{
    if (!m_isConnected)
        return;

    m_lastUsed.restart();

    m_connection.Close(m_stream);
}


bool QnColdStoreConnection::seek(qint64 pos)
{
    if (!m_isConnected)
        return false;

    m_lastUsed.restart();

    if (m_connection.Seek(
        m_stream, 
        pos, 
        0, //0 – Do step 1 (file_offset is relative to beginning of file).
        0, //If (seek_whence != 0xff) then this parameter is ignored.
        0, //If (seek_direction == 0xff) then this parameter is ignored
        0xff //seek_direction 0xff – Skip step 2
        ) != Veracity::ISFS::STATUS_SUCCESS)
    {
        Q_ASSERT(false);
        return false;
    }

    return true;
}

bool QnColdStoreConnection::write(const char* data, int len)
{
    if (!m_isConnected)
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
        Q_ASSERT(false);
        return false;
    }

    m_pos += returned_data_length;

    Q_ASSERT(returned_data_length == len);

    return true;
}


int QnColdStoreConnection::read(char *data, int len)
{
    if (!m_isConnected)
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


    return readed;
}

int QnColdStoreConnection::age() const
{
    return m_lastUsed.elapsed()/1000;
}

QString QnColdStoreConnection::getFilename() const
{
    return m_filename;
}

qint64 QnColdStoreConnection::pos() const
{
    return m_pos;
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