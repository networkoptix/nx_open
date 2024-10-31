// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/storage_resource.h>
#include <nx/vms/api/data/storage_init_result.h>

namespace nx {

class NX_VMS_COMMON_API StorageResourceStub: public QnStorageResource
{
    using base_type = QnStorageResource;
public:
    StorageResourceStub();
    virtual ~StorageResourceStub();

    virtual nx::vms::api::StorageInitResult initOrUpdate() override;

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
protected:
    virtual QIODevice* openInternal(const QString& fileName, QIODevice::OpenMode openMode) override;
};

} // namespace nx
