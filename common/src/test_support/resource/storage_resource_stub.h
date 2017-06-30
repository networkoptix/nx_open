#pragma once

#include <core/resource/storage_resource.h>

namespace nx {

class StorageResourceStub: public QnStorageResource
{
    using base_type = QnStorageResource;
public:
    StorageResourceStub();
    virtual ~StorageResourceStub();

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;
    virtual Qn::StorageInitResult initOrUpdate() override;

    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() const override;
    virtual int getCapabilities() const override;
};

} // namespace nx
