#ifndef __CLIENT_STORAGE_RESOURCE_H__
#define __CLIENT_STORAGE_RESOURCE_H__

#include <core/resource/storage_resource.h>
/*
*   This class used only for manipulating storage as User Interface entity. 
*   Never use any functions derived from QnAbstractStorageResource.
*/
class QnClientStorageResource
    : public QnStorageResource
{
public:
    enum StorageFlags {
        ReadOnly        = 0x1,
        ContainsCameras = 0x2,
    };

    QnClientStorageResource() {}
    virtual ~QnClientStorageResource() {}

    static QnStorageResource* instance();

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getCapabilities() const override;
    virtual bool isAvailable() const override;
    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() override;
};

typedef QSharedPointer<QnClientStorageResource> QnClientStorageResourcePtr;

#endif // __CLIENT_STORAGE_RESOURCE_H__
