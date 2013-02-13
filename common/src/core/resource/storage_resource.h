#ifndef QN_STORAGE_RESOURCE_H
#define QN_STORAGE_RESOURCE_H

#include <QSet>
#include "resource.h"
#include <QFileInfoList>
#include <QIODevice>
#include "abstract_storage_resource.h"

class QnStorageResource : public QnAbstractStorageResource
{
    Q_OBJECT

public:
    QnStorageResource();
    virtual ~QnStorageResource();

public:
    //virtual void registerFfmpegProtocol() const = 0;
    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) = 0;


    virtual int getChunkLen() const = 0;

    /*
    *   If function returns true, auto delete old files
    */
    virtual bool isNeedControlFreeSpace() = 0;

    /*
    *   Return storage free space. if isNeedControlFreeSpace returns false, this function is not used
    */
    virtual qint64 getFreeSpace() = 0;

    /*
    *   Return storage total space. if isNeedControlFreeSpace returns false, this function is not used
    */
    virtual qint64 getTotalSpace() = 0;

    /*
    *   Returns true if storage physically acccesible right now (ready only check)
    */
    virtual bool isStorageAvailable() = 0;

    /*
    *   Returns true if storage physically acccesible and ready for writing
    */
    virtual bool isStorageAvailableForWriting() = 0;


    /*
    *   Remove file from storage
    */
    virtual bool removeFile(const QString& url) = 0;

    /*
    *   Remove dir from storage
    */
    virtual bool removeDir(const QString& url) = 0;

    /*
    *   This function is used when server restarts. Unfinished files re-readed, writed again (under a new name), then renamed.
    */
    virtual bool renameFile(const QString& oldName, const QString& newName) = 0;

    /*
    *   Returns file list in current directory, only files (but not subdirs) requires.
    *   QFileInfo structure MUST contains valid fields for full file name and file size.
    */
    virtual QFileInfoList getFileList(const QString& dirName) = 0;

    /*
    *   Returns true if file exists
    */
    virtual bool isFileExists(const QString& url) = 0;

    /*
    *   Returns true if dir exists
    */
    virtual bool isDirExists(const QString& url) = 0;

    /*
    *   Returns true if storage support catalog functions: isFileExists, isDirExists, getFileList. 
    *   If function returns false, server doesn't check file catalog on startup
    */
    virtual bool isCatalogAccessible() = 0;

    virtual bool isRealFiles() const{return true;}

    //qint64 getWritedSpace() const;
    //void addWritedSpace(qint64 value);

    virtual qint64 getFileSize(const QString& fillName) const = 0;
public:
    virtual void setUrl(const QString& value);
protected:
    //qint64 m_writedSpace;
private:
    mutable QMutex m_writedSpaceMtx;
};

typedef QnStorageResource* (*StorageTypeInstance)();

class QnStoragePluginFactory
{
public:
    QnStoragePluginFactory();
    virtual ~QnStoragePluginFactory();
    static QnStoragePluginFactory* instance();

    void registerStoragePlugin(const QString& name, StorageTypeInstance pluginInst, bool isDefaultProtocol = false);
    QnStorageResource* createStorage(const QString& storageType, bool useDefaultForUnknownPrefix = true);

private:
    QMap<QString, StorageTypeInstance> m_storageTypes;
    StorageTypeInstance m_defaultStoragePlugin;
    QMutex m_mutex;
};

#endif // QN_STORAGE_RESOURCE_H
