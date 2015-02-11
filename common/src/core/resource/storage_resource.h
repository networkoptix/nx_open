#ifndef QN_STORAGE_RESOURCE_H
#define QN_STORAGE_RESOURCE_H

#include <QtCore/QSet>
#include "resource.h"
#include <QtCore/QFileInfoList>
#include <QtCore/QIODevice>
#include "abstract_storage_resource.h"

class QnStorageResource : public QnAbstractStorageResource
{
    Q_OBJECT

public:
    QnStorageResource();
    virtual ~QnStorageResource();

public:
    static const qint64 UnknownSize = 0x0000FFFFFFFFFFFFll; // TODO: #Elric replace with -1.

    virtual QIODevice *open(const QString &fileName, QIODevice::OpenMode openMode) = 0;

    /**
     * TODO: #vasilenko doxydocs!
     */
    virtual int getChunkLen() const = 0;

    /**
     * \returns                         Whether the storage automatically deletes old files.
     */
    virtual bool isNeedControlFreeSpace() = 0;

    /**
     * \returns                         Storage free space in bytes, or <tt>UnknownSize</tt> if this function is not supported.
     */
    virtual qint64 getFreeSpace() = 0;

    /**
     * \returns                         Storage total space in bytes, or <tt>UnknownSize</tt> if this function is not supported.
     */
    virtual qint64 getTotalSpace() = 0;

    /**
     * \returns                         Whether the storage is physically accessible.
     */
    virtual bool isStorageAvailable() = 0;

    /**
     * \returns                         Whether the storage is physically accessible and ready for writing
     */
    virtual bool isStorageAvailableForWriting() = 0;


    /**
     * \param url                       Url of the file to delete.
     * \returns                         Whether the file was successfully removed.
     */
    virtual bool removeFile(const QString& url) = 0;

    /**
     * \param url                       Url of the folder to remove.
     * \returns                         Whether the folder was successfully removed.
     */
    virtual bool removeDir(const QString& url) = 0;

    /**
     * This function is used when server restarts. Unfinished files re-readed, writed again (under a new name), then renamed.
     *
     * \param oldName
     * \param newName
     * \returns                         Whether the file was successfully renamed.
     */
    virtual bool renameFile(const QString& oldName, const QString& newName) = 0;

    /**
     * \param dirName                   Url of the folder to list.
     * \returns                         List of files in given directory, excluding subdirectories.
     * 
     * \note QFileInfo structure MUST contains valid fields for full file name and file size.
     */
    virtual QFileInfoList getFileList(const QString& dirName) = 0;

    /**
     * \param url                       Url of a file to check.
     * \returns                         Whether a file with the given url exists.
     */
    virtual bool isFileExists(const QString& url) = 0;

    /**
     * \param url                       Url of a folder to check.
     * \returns                         Whether a folder with the given name exists.
     */
    virtual bool isDirExists(const QString& url) = 0;

    /**
     * \returns                         Whether storage supports catalog functions: 
     *                                  <tt>isFileExists</tt>, <tt>isDirExists</tt>, <tt>getFileList</tt>. 
     *                                  If function returns false, server doesn't check file catalog on startup.
     */
    virtual bool isCatalogAccessible() = 0;

    /**
     * TODO: #vasilenko doxydocs!
     */
    virtual bool isRealFiles() const{return true;}

    //qint64 getWritedSpace() const;
    //void addWritedSpace(qint64 value);

    /**
     * \param url                       Url of the file to get size of.
     * \returns                         Size of the file, or 0 if the file does not exist.
     */
    virtual qint64 getFileSize(const QString& url) const = 0;
protected:
    //qint64 m_writedSpace;

private:
    mutable QnMutex m_writedSpaceMtx;
};


class QnStoragePluginFactory
{
public:
    typedef QnStorageResource *(*StorageResourceFactory)();

    QnStoragePluginFactory();
    virtual ~QnStoragePluginFactory();
    static QnStoragePluginFactory *instance();

    void registerStoragePlugin(const QString &protocol, const StorageResourceFactory &factory, bool isDefaultProtocol = false);
    QnStorageResource *createStorage(const QString &url, bool useDefaultForUnknownPrefix = true);

private:
    QHash<QString, StorageResourceFactory> m_factoryByProtocol;
    StorageResourceFactory m_defaultFactory;
    QnMutex m_mutex; // TODO: #vasilenko this mutex is not used, is it intentional?
};

#endif // QN_STORAGE_RESOURCE_H
