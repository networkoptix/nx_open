#include "coldstore_storage.h"

QnPlColdStoreStorage::QnPlColdStoreStorage(const QString& storageLink, int minFreeSpace)
{
#ifdef Q_OS_WIN
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(1, 1);
    WSAStartup(wVersionRequested, &wsaData);
#endif
    const char* ip = "192.168.1.30";


    m_csConnection = new Veracity::SFSClient();

    if (m_csConnection->Connect(ip) != Veracity::ISFS::STATUS_SUCCESS)
        return ;

    int n = 0;

}

QnPlColdStoreStorage::~QnPlColdStoreStorage()
{

}

QString QnPlColdStoreStorage::getName() const
{
    return "coldstore";
}

STORAGE_FILE_HANDLER QnPlColdStoreStorage::open(const QString& fname)
{
    if (!m_csConnection)
        return 0;

    
}

void QnPlColdStoreStorage::close(STORAGE_FILE_HANDLER)
{

}

void QnPlColdStoreStorage::flush(STORAGE_FILE_HANDLER)
{

}

int QnPlColdStoreStorage::write(const char* data, int size)
{
    return 0;
}

int QnPlColdStoreStorage::read(char* data, int size)
{
    return 0;
}

int QnPlColdStoreStorage::seek(int shift)
{
    return 0;
}
