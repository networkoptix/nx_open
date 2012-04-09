#ifndef cold_store_storage_h_1800
#define cold_store_storage_h_1800

#include "coldstore_api/sfs-client.h"

typedef qint64 STORAGE_FILE_HANDLER;

class QnPlColdStoreStorage //: public SomeStorageInterface
{
public:
    QnPlColdStoreStorage(const QString& storageLink, int minFreeSpace = 10);
    virtual ~QnPlColdStoreStorage();
    QString getName() const;
    
    // returns file handler 
    STORAGE_FILE_HANDLER create(const QString& fname); // something to think about may be should be a structure with y m d h m channel_id, low/h stream and so on
    STORAGE_FILE_HANDLER open(const QString& fname); // something to think about may be should be a structure with y m d h m channel_id, low/h stream and so on
    void close(STORAGE_FILE_HANDLER);
    void flush(STORAGE_FILE_HANDLER);
    int write(const char* data, int size);
    int read(char* data, int size);
    int seek(int shift);

private:

    Veracity::ISFS* m_csConnection;
    Veracity::u32 m_stream;

};


#endif //cold_store_storage_h_1800