/***********************************************************************
*	File: file.h
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#ifndef _FS_FILE_H_
#define _FS_FILE_H_

#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <fcntl.h>

#ifdef WIN32
#pragma warning( disable : 4290 )
#endif


class QN_EXPORT QnFile
{
public:
    QnFile();
    QnFile(const QString& fName);
    virtual ~QnFile();
    void setFileName(const QString& fName) { m_fileName = fName; }
    QString getFileName() const { return m_fileName; }

    virtual bool open(const QIODevice::OpenMode& mode, unsigned int systemDependentFlags = 0);
    virtual void close();
    virtual qint64 read(char* buffer, qint64 count);

    virtual qint64 write(const char* buffer, qint64 count);
    virtual void sync();
    virtual bool isOpen() const;
    virtual qint64 size() const;
    virtual bool seek( qint64 offset);
    virtual bool truncate( qint64 newFileSize);

    //!Returns true if file system entry with name \a fileName exists
    static bool fileExists( const QString& fileName );
    //!Retrieves file size
    /*!
        \return file size in bytes. -1 if file size could not be read (no file, access denied, \a fileName is a directory etc...)
    */
    static qint64 getFileSize( const QString& fileName );
    //!If \a path /etc/smb.conf, returns /etc/, if /etc/ returns /etc/
    static QString filePath( const QString& path );
    //!If \a path /etc/smb.conf, returns smb.conf
    static QString fileName( const QString& path );
    //!If \a path /etc/smb.conf, returns smb
    static QString baseName( const QString& path );
    static QString absolutePath( const QString& path );

protected:
    QString m_fileName;

private:
    void* m_impl;
};

#endif	//_FS_FILE_H_
