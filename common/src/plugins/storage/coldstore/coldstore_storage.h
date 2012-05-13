#ifndef cold_store_storage_h_1800
#define cold_store_storage_h_1800

#include "coldstore_api/sfs-client.h"
#include "libavformat/avio.h"
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
    virtual bool isNeedControlFreeSpace() override;

    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual bool removeFile(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;

    QString coldstoreAddr() const;

    void onWriteBuffClosed(QnColdStoreIOBuffer* buff);

    void onWrite(const QByteArray& ba, const QString& fn);

    //=====================
    QString fileName2csFileName(const QString& fn) const;
    QnCSFileInfo getFileInfo(const QString& fn);
private:
    QnColdStoreMetaDataPtr getMetaDataFileForCsFile(const QString& csFile);
    QString csDataFileName(const QnStorageURL& url) const;

    bool hasOpenFilesFor(const QString& csFile) const;

    QnColdStoreConnection* getPropriteConnectionForCsFile(const QString& csFile);

private:
    mutable QMutex m_mutex;

    QnColdStoreConnectionPool m_connectionPool;

    QnColdStoreMetaDataPool m_metaDataPool;

    QnColdStoreMetaDataPtr m_currentWritingFileMetaData;
    QnColdStoreMetaDataPtr m_prevWritingFileMetaData;

    QnColdStoreWriter* m_cswriterThread;
    QSet<QString> m_listOfWritingFiles;


    QnColdStoreConnection* m_currentConnection;
    QnColdStoreConnection* m_prevConnection;
};


#endif //cold_store_storage_h_1800