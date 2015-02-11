#ifndef coldstore_connection_pool_h1931
#define coldstore_connection_pool_h1931

#ifdef ENABLE_COLDSTORE

#include "coldstore_api/sfs-client.h"

#include <QHash>
#include <QtCore/QIODevice>
#include <QtCore/QTime>
#include <utils/common/mutex.h>


#define CS_ACTUAL_DATA_CHANNEL 0
#define CS_META_DATA_CHANNEL 1

class QnPlColdStoreStorage;

class QnColdStoreConnection
{
public:
    QnColdStoreConnection(const QString& addr);
    virtual ~QnColdStoreConnection();

    qint64 oldestFileTime(const QString& fn);

    bool open(const QString& fn, QIODevice::OpenModeFlag flag, int channel);

    qint64 size() const; // returns the size of opened channel

    void close();


    bool seek(qint64 pos);
    bool write(const char* data, int len);
    int read(char *data, int len);

    int age() const; // return the number of seconds then the connection was used last time 

    QString getFilename() const;

    qint64 pos() const;

    void externalLock() const;
    void externalUnLock() const;

private:
    Veracity::SFSClient m_connection;
    Veracity::u32 m_stream;

    QTime m_lastUsed;

    bool m_isConnected;

    QIODevice::OpenModeFlag m_openMode;

    Veracity::u64 m_openedStreamSize;

    QString m_filename;

    qint64 m_pos;

    int m_channel;

    bool m_opened;

    mutable QnMutex m_mutex;
        
};

//================================================================
// the pool is used for read only.
// all writes to coldstore goes through just one connection.
class QnColdStoreConnectionPool
{
public:
    QnColdStoreConnectionPool(QnPlColdStoreStorage *csStorage);
    virtual ~QnColdStoreConnectionPool();

    int read(const QString& csFn,   char* data, quint64 shift, int len);

private:
    void checkIfSomeConnectionsNeedToBeClosed();
private:

    QnPlColdStoreStorage *m_csStorage;
    
    QHash<QString, QnColdStoreConnection*>  m_pool;

    QnMutex m_mutex;
};

#endif // ENABLE_COLDSTORE

#endif // coldstore_connection_pool_h1931
