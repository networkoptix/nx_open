#ifndef cold_store_storage_h_1800
#define cold_store_storage_h_1800

#include "coldstore_api/sfs-client.h"
#include "libavformat/avio.h"
#include "core/resource/storage_resource.h"

typedef qint64 STORAGE_FILE_HANDLER;

class QnPlColdStoreStorage : public QnStorageResource
{
public:
    QnPlColdStoreStorage();

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getChunkLen() const override;
    virtual bool isStorageAvailable() override;
    virtual bool isFolderAvailableForWriting(const QString& path) override;
    virtual qint64 getFreeSpace() override;
    virtual bool isNeedControlFreeSpace() override;

    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual bool removeFile(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;

private:

    Veracity::ISFS* m_csConnection;
    Veracity::u32 m_stream;

};


#endif //cold_store_storage_h_1800