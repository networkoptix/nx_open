#ifndef __ABSTRACT_STORAGE_H__
#define __ABSTRACT_STORAGE_H__

#include "resource.h"
#include <QtCore/QtCore>

class QnAbstractStorageResource 
    : public QnResource 
{
public:
    enum cap
    {
        ListFile        = 0x0000,                   // capable of listing files
        RemoveFile      = 0x0001,                   // capable of removing files
        ReadFile        = 0x0002,                   // capable of reading files
        WriteFile       = 0x0004,                   // capable of writing files
        DBReady         = 0x0008,                   // capable of DB hosting
    };

    static const int chunkLen = 60;
public:
    static const qint64 UnknownSize = 0x0000FFFFFFFFFFFFll; // TODO: #Elric replace with -1.

    virtual QIODevice *open(const QString &fileName, QIODevice::OpenMode openMode) = 0;

    /**
    *   \return storage capabilities ('cap' flag(s))
    */
    virtual int getCapabilities() const = 0;

    ///**
    // * TODO: #vasilenko doxydocs!
    // */
    //virtual int getChunkLen() const = 0;

    ///**
    // * \returns                         Whether the storage automatically deletes old files.
    // */
    //virtual bool isNeedControlFreeSpace() = 0;

    /**
     * \returns                         Storage free space in bytes, or <tt>UnknownSize</tt> if this function is not supported.
     */
    virtual qint64 getFreeSpace() = 0;

    /**
     * \returns                         Storage total space in bytes, or <tt>UnknownSize</tt> if this function is not supported.
     */
    virtual qint64 getTotalSpace() = 0;

    ///**
    // * \returns                         Whether the storage is physically accessible.
    // */
    virtual bool isAvailable() const = 0;

    ///**
    // * \returns                         Whether the storage is physically accessible and ready for writing
    // */
    //virtual bool isStorageAvailableForWriting() = 0;


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

    ///**
    // * \returns                         Whether storage supports catalog functions: 
    // *                                  <tt>isFileExists</tt>, <tt>isDirExists</tt>, <tt>getFileList</tt>. 
    // *                                  If function returns false, server doesn't check file catalog on startup.
    // */
    //virtual bool isCatalogAccessible() = 0;

    ///**
    // * TODO: #vasilenko doxydocs!
    // */
    //virtual bool isRealFiles() const{return true;}

    //qint64 getWritedSpace() const;
    //void addWritedSpace(qint64 value);

    /**
     * \param url                       Url of the file to get size of.
     * \returns                         Size of the file, or 0 if the file does not exist.
     */
    virtual qint64 getFileSize(const QString& url) const = 0;
};

#endif // __ABSTRACT_STORAGE_H__
