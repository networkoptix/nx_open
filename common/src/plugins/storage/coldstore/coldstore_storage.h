#ifndef cold_store_storage_h_1800
#define cold_store_storage_h_1800

#include "coldstore_api/sfs-client.h"
#include "libavformat/avio.h"
#include "core/resource/storage_resource.h"


class QnPlColdStoreStorage : public QnStorageResource
{
public:
    QnPlColdStoreStorage();

    static QnStorageResource* instance();

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getChunkLen() const override;
    virtual bool isStorageAvailable() override;
    virtual qint64 getFreeSpace() override;
    virtual bool isNeedControlFreeSpace() override;

    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual bool removeFile(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;

private:
    QString coldstoreAddr() const;

    QString csDataFileName(const QnStorageURL& url) const;
private:
    mutable QMutex m_mutex;


};


#endif //cold_store_storage_h_1800