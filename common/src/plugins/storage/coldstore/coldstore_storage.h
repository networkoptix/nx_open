#ifndef cold_store_storage_h_1800
#define cold_store_storage_h_1800

#ifdef ENABLE_COLDSTORE

#include "coldstore_api/sfs-client.h"

extern "C"
{
    #include "libavformat/avio.h"
}

#include "core/resource/storage_resource.h"
#include "coldstore_connection_pool.h"
#include "coldstore_storage_helper.h"
#include "coldstore_writer.h"

class QnColdStoreIOBuffer;



class QnPlColdStoreStorage : public QnStorageResource
{
public:
    QnPlColdStoreStorage();
    ~QnPlColdStoreStorage();

    static QnStorageResource* instance();

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getChunkLen() const override;
    virtual bool isStorageAvailable() override;
    virtual bool isStorageAvailableForWriting() override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() override;
    virtual bool isNeedControlFreeSpace() override;

    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual qint64 getFileSize(const QString& url) const override;

    virtual bool removeFile(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual bool isRealFiles() const override {return false;};
    virtual bool isCatalogAccessible() override;

    //=====================
    QString coldstoreAddr() const;
    void onWriteBuffClosed(QnColdStoreIOBuffer* buff);
    void onWrite(const QByteArray& ba, const QString& fn);

protected:
    
private:

    QString normolizeFileName(const QString& fn) const;

    QString fileName2csFileName(const QString& fn) const;

    QnColdStoreMetaDataPtr getMetaDataFileForCsFile(const QString& csFile);
    bool hasOpenFilesFor(const QString& csFile) const;
    QnCsTimeunitConnectionHelper* getPropriteConnectionForCsFile(const QString& csFile);
    QnCSFileInfo getFileInfo(const QString& fn);

    qint64 getOldestFileTime();

    void checkIfRangeNeedsToBeUpdated();

private:



    mutable QnMutex m_mutex;
    QnColdStoreConnectionPool m_connectionPool;
    QnColdStoreMetaDataPool m_metaDataPool;

    QnColdStoreWriter* m_cswriterThread;

    QSet<QString> m_listOfWritingFiles;
    //QSet<QString> m_listOfExistingFiles;

    QnCsTimeunitConnectionHelper* m_currH;
    QnCsTimeunitConnectionHelper* m_prevH;

    QTime m_lastRangeUpdate;
    bool m_RangeUpdatedAtLeastOnce;

};

#endif // ENABLE_COLDSTORE

#endif //cold_store_storage_h_1800
