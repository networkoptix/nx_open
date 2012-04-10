#ifndef cold_store_storage_h_1800
#define cold_store_storage_h_1800

#include "coldstore_api/sfs-client.h"
#include "core/storage_protocol/abstract_storage_protocol.h"
#include "libavformat/avio.h"

typedef qint64 STORAGE_FILE_HANDLER;

class QnPlColdStoreStorage : public QnAbstractStorageProtocol
{
public:
    QnPlColdStoreStorage();

    virtual URLProtocol getURLProtocol() const override;

    virtual int getChunkLen() const override;
    virtual bool isStorageAvailable(const QString& value) override;
    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual bool isNeedControlFreeSpace() override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace(const QString& url) override;

private:

    Veracity::ISFS* m_csConnection;
    Veracity::u32 m_stream;

};


#endif //cold_store_storage_h_1800