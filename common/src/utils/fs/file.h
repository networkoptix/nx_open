/***********************************************************************
*	File: file.h
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#ifndef _FS_FILE_H_
#define _FS_FILE_H_

#include <memory>

#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <fcntl.h>

#include <nx/utils/system_error.h>

#ifdef WIN32
#pragma warning( disable : 4290 )
#endif


class IQnFile
{
public:
    virtual QString getFileName() const = 0;

    virtual bool open(
        const QIODevice::OpenMode   &mode,
        unsigned int                systemDependentFlags = 0
    ) = 0;

    virtual void close() = 0;
    virtual qint64 read(char* buffer, qint64 count) = 0;

    virtual qint64 write(const char* buffer, qint64 count) = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 size() const = 0;
    virtual bool seek( qint64 offset) = 0;
    virtual bool truncate( qint64 newFileSize) = 0;
    virtual bool eof() const { return true; }

    virtual ~IQnFile() {}
};

class QN_EXPORT QnFile
:
    public IQnFile,
    public std::enable_shared_from_this<QnFile>
{
public:
    class AbstractWriteHandler
    {
    public:
        virtual ~AbstractWriteHandler() {}

        /*!
            \param bytesWritten Can be valid even if \a errorCode is not \a SystemError::noError
            \param errorCode Different from \a SystemError::noError if error occured
        */
        virtual void onAsyncWriteFinished(
            const std::shared_ptr<QnFile>& file,
            uint64_t bytesWritten,
            SystemError::ErrorCode errorCode ) = 0;
    };

    class AbstractCloseHandler
    {
    public:
        virtual ~AbstractCloseHandler() {}

        /*!
            File is closed even if error occured
            \param errorCode Different from \a SystemError::noError if error occured
        */
        virtual void onAsyncCloseFinished(
            const std::shared_ptr<QnFile>& file,
            SystemError::ErrorCode errorCode ) = 0;
    };

    QnFile();
    QnFile(const QString& fName);
    virtual ~QnFile();
    void setFileName(const QString& fName) { m_fileName = fName; }
    virtual QString getFileName() const { return m_fileName; }

    virtual bool open(const QIODevice::OpenMode& mode, unsigned int systemDependentFlags = 0);
    virtual void close();
    virtual qint64 read(char* buffer, qint64 count);
    virtual bool eof() const override;

    /*!
        \return Bytes written or -1 in case of error (use \a SystemError::getLastOSErrorCode() to get error code)
    */
    virtual qint64 write(const char* buffer, qint64 count);
    virtual void sync();
    virtual bool isOpen() const;
    virtual qint64 size() const;
    virtual bool seek( qint64 offset);
    virtual bool truncate( qint64 newFileSize);

    //!Starts asynchronous write call. On completion \a handler->onAsyncWriteFinished() will be called
    /*!
        \note \a QnFile instance must be used with \a std::shared_ptr
        \note Not all bytes of \a buffer can be written only in case of error
        \return false if failed to start asynchronous call
    */
    bool writeAsync( const QByteArray& buffer, AbstractWriteHandler* handler );
    //!Starts asynchronous close call. On completion \a handler->onAsyncCloseFinished() will be called
    /*!
        \note \a QnFile instance must be used with \a std::shared_ptr
        \return false if failed to start asynchronous call
    */
    bool closeAsync( AbstractCloseHandler* handler );

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
    bool m_eof;
};

#endif	//_FS_FILE_H_
