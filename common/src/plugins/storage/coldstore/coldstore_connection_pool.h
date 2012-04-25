#ifndef coldstore_connection_pool_h1931
#define coldstore_connection_pool_h1931
#include "coldstore_api/sfs-client.h"



class QnColdStoreConnection
{
public:
    QnColdStoreConnection(const QString& addr);
    virtual ~QnColdStoreConnection();

    bool open(const QString& fn, QIODevice::OpenModeFlag flag);
    void close();


    bool seek(qint64 pos);
    bool write(const char* data, int len);
    int read(char *data, int len);

private:
    Veracity::SFSClient m_connection;
    Veracity::u32 m_stream;

    bool m_isConnected;
        
};

//================================================================
// the pool is used for read only.
// all writes to coldstore goes through just one connection.
class QnColdStoreConnectionPool
{
public:
    QnColdStoreConnectionPool();
    virtual ~QnColdStoreConnectionPool(const QString& addr);
    int read(const QString& csFn,   char* data, quint64 shift, int len);
private:
    QString m_csAddr;
};

#endif // coldstore_connection_pool_h1931