/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#ifndef EXT_IODEVICE_STORAGE_H
#define EXT_IODEVICE_STORAGE_H

#include <map>

#include <utils/common/mutex.h>

#include "core/resource/storage_resource.h"


//!Provides externally-supplied QIODevice as resource
class QnExtIODeviceStorageResource
:
    public QnStorageResource
{
public:
    QnExtIODeviceStorageResource();
    /*!
        Destroys any registered data
    */
    virtual ~QnExtIODeviceStorageResource();

public:
    //!Implementation of QnStorageResource::open
    /*!
        Returns already created IO device with path \a fileName.
        \note Ownership of returned object is passed to the caller
        \note Returned object is no longer accessible via this storage
    */
    virtual QIODevice* open( const QString& fileName, QIODevice::OpenMode openMode ) override;

    //!Implementation of QnStorageResource::getChunkLen
    virtual int getChunkLen() const override { return 0; }
    //!Implementation of QnStorageResource::isNeedControlFreeSpace
    virtual bool isNeedControlFreeSpace() override { return false; }
    //!Implementation of QnStorageResource::getFreeSpace
    virtual qint64 getFreeSpace() override { return 0; }
    //!Implementation of QnStorageResource::getTotalSpace
    virtual qint64 getTotalSpace() override { return 0; }
    //!Implementation of QnStorageResource::isStorageAvailable
    virtual bool isStorageAvailable() override { return true; }
    //!Implementation of QnStorageResource::isStorageAvailableForWriting
    virtual bool isStorageAvailableForWriting() override { return false; }
    //!Implementation of QnStorageResource::removeFile
    virtual bool removeFile( const QString& path ) override;
    //!Implementation of QnStorageResource::removeDir
    virtual bool removeDir( const QString& path ) override { Q_UNUSED(path) return false; }
    //!Implementation of QnStorageResource::renameFile
    virtual bool renameFile( const QString& oldName, const QString& newName ) override { Q_UNUSED(oldName) Q_UNUSED(newName) return false; }
    //!Implementation of QnStorageResource::getFileList
    virtual QFileInfoList getFileList( const QString& dirName );
    //!Implementation of QnStorageResource::isFileExists
    virtual bool isFileExists( const QString& path );
    //!Implementation of QnStorageResource::isDirExists
    virtual bool isDirExists( const QString& path ) override { Q_UNUSED(path) return false; }
    //!Implementation of QnStorageResource::isCatalogAccessible
    virtual bool isCatalogAccessible() override { return true; }
    //!Implementation of QnStorageResource::isRealFiles
    virtual bool isRealFiles() const{ return false; }
    //!Implementation of QnStorageResource::getFileSize
    virtual qint64 getFileSize( const QString& path ) const override;

    /*!
        Takes ownership of \a data.
        If \a path is already used, old data is destroyed and replaced by a new one
    */
    void registerResourceData( const QString& path, QIODevice* data );

private:
    std::map<QString, QIODevice*> m_urlToDevice;
    mutable QnMutex m_mutex;
};

typedef QnSharedResourcePointer<QnExtIODeviceStorageResource> QnExtIODeviceStorageResourcePtr;

#endif  //EXT_IODEVICE_STORAGE_H
