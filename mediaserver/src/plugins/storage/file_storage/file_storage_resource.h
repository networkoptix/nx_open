#ifndef _FILE_STORAGE_PROTOCOL_H__
#define _FILE_STORAGE_PROTOCOL_H__

#include <libavformat/avio.h>
#include "core/resource/storage_resource.h"

/*
* QnFileStorageResource uses custom implemented IO access
*/

class QnFileStorageResource: public QnStorageResource
{
public:
    QnFileStorageResource();
    ~QnFileStorageResource();

    static QnStorageResource* instance();

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getChunkLen() const override;

    virtual float getAvarageWritingUsage() const override;

    virtual bool isStorageAvailable() override;
    virtual bool isStorageAvailableForWriting() override;

    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual qint64 getFileSize(const QString& url) const override;
    virtual bool isNeedControlFreeSpace() override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    bool isCatalogAccessible() override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() override;


    void setStorageBitrateCoeff(float value);
    virtual float getStorageBitrateCoeff() const override;

    virtual void setUrl(const QString& url) override;

private:
    QString removeProtocolPrefix(const QString& url);
private:
    // used for 'virtual' storage bitrate. If storage has more free space, increase 'virtual' storage bitrate for full storage space filling
    float m_storageBitrateCoeff;

    bool isStorageDirMounted();
};
typedef QSharedPointer<QnFileStorageResource> QnFileStorageResourcePtr;

#endif // _FILE_STORAGE_PROTOCOL_H__
