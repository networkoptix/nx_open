// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/utils/thread/mutex.h>

#include "core/resource/storage_resource.h"

//!Provides externally-supplied QIODevice as resource
class NX_VMS_COMMON_API QnExtIODeviceStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
public:
    QnExtIODeviceStorageResource();
    /*!
        Destroys any registered data
    */
    virtual ~QnExtIODeviceStorageResource();

public:
    //!Implementation of QnStorageResource::open

    //!Implementation of QnStorageResource::getFreeSpace
    virtual qint64 getFreeSpace() override { return 0; }
    //!Implementation of QnStorageResource::getTotalSpace
    virtual qint64 getTotalSpace() const override { return 0; }
    //!Implementation of QnStorageResource::isStorageAvailable
    virtual Qn::StorageInitResult initOrUpdate() override { return Qn::StorageInit_Ok; }
    //!Implementation of QnStorageResource::removeFile
    virtual bool removeFile( const QString& path ) override;
    //!Implementation of QnStorageResource::removeDir
    virtual bool removeDir( const QString& /*path*/ ) override { return false; }
    //!Implementation of QnStorageResource::renameFile
    virtual bool renameFile( const QString& /*oldName*/, const QString& /*newName*/ ) override { return false; }
    //!Implementation of QnStorageResource::getFileList
    virtual QnAbstractStorageResource::FileInfoList getFileList( const QString& dirName ) override;
    //!Implementation of QnStorageResource::isFileExists
    virtual bool isFileExists( const QString& path ) override;
    //!Implementation of QnStorageResource::isDirExists
    virtual bool isDirExists( const QString& /*path*/ ) override { return false; }
    //!Implementation of QnStorageResource::isRealFiles
    virtual bool isRealFiles() const{ return false; }
    //!Implementation of QnStorageResource::getFileSize
    virtual qint64 getFileSize( const QString& path ) const override;

    virtual int getCapabilities() const override;
    /*!
        Takes ownership of \a data.
        If \a path is already used, old data is destroyed and replaced by a new one
    */
    void registerResourceData( const QString& path, QIODevice* data );

    void setIsIoDeviceOwner(bool isIoDeviceOwner);
protected:
    /**
     * Returns already created IO device with path \a fileName.
     *note Ownership of returned object is passed to the caller
     *note Returned object is no longer accessible via this storage
    */
    virtual QIODevice* openInternal( const QString& fileName, QIODevice::OpenMode openMode ) override;
private:
    std::map<QString, QIODevice*> m_urlToDevice;
    mutable nx::Mutex m_mutex;
    int m_capabilities;
    bool m_isIoDeviceOwner = true;
};

typedef QnSharedResourcePointer<QnExtIODeviceStorageResource> QnExtIODeviceStorageResourcePtr;
