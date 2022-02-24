// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>
#include <string>

#include <QtCore/QtGlobal>
#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <fcntl.h>

#include <nx/utils/system_error.h>

#include "async_file_processor.h"

#ifdef _WIN32
#pragma warning( disable : 4290 )
#endif

#if defined (Q_OS_LINUX) || defined (Q_OS_MAC)
    NX_UTILS_API int makeUnixOpenFlags(const QIODevice::OpenMode& oflag);
#endif

/**
 * Abstract file class.
 */
class NX_UTILS_API IQnFile
{
public:
    virtual ~IQnFile() = default;

    virtual bool open(
        const QIODevice::OpenMode& mode,
        unsigned int systemDependentFlags = 0) = 0;

    virtual void close() = 0;
    virtual qint64 read(char* buffer, qint64 count) = 0;

    virtual qint64 write(const char* buffer, qint64 count) = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 size() const = 0;
    virtual bool seek(qint64 offset) = 0;
    virtual bool truncate(qint64 newFileSize) = 0;
    virtual bool eof() const { return true; }
    virtual QString url() const = 0;
    /**
     * Reads opened file to the end.
     * @return std::nullopt in case of an error.
     * Use SystemError::getLastOsErrorCode() to get error details.
     */
    std::optional<nx::Buffer> readAll();
};

//-------------------------------------------------------------------------------------------------

namespace nx::utils::fs {

/**
 * A file implementation.
 * Internally uses either QnFile (when working with regular files) or
 * QFile (for Qt resource files).
 * NOTE: Asynchronous functions are not effecient and should not be used where
 * performance is a concern.
 * Asynchronous calls may be scheduled without waiting for the last one to complete.
 * They will be queued and processed in the order.
 */
class NX_UTILS_API File:
    public IQnFile
{
public:
    File(const std::string& filename);

    virtual bool open(
        const QIODevice::OpenMode& mode,
        unsigned int systemDependentFlags = 0) override;

    virtual void close() override;
    virtual qint64 read(char* buffer, qint64 count) override;

    virtual qint64 write(const char* buffer, qint64 count) override;
    virtual bool isOpen() const override;
    virtual qint64 size() const override;
    virtual bool seek(qint64 offset) override;
    virtual bool truncate(qint64 newFileSize) override;
    virtual bool eof() const override;
    virtual QString url() const override;
    /**
     * @param systemDependentFlags Usually, zero.
     */
    void openAsync(
        nx::utils::fs::FileAsyncIoScheduler* scheduler,
        const QIODevice::OpenMode& mode,
        unsigned int systemDependentFlags,
        nx::utils::fs::OpenHandler handler);

    /**
     * Reads maximum (buf->capacity() - buf->size()) bytes and appends them to buf.
     * Read is done from the current file position.
     * @param buf MUST be valid until request completion.
     * @param handler Invoked on completion.
     */
    void readAsync(
        nx::utils::fs::FileAsyncIoScheduler* scheduler,
        nx::Buffer* buf,
        nx::utils::fs::IoCompletionHandler handler);

    /**
     * Reads opened file to the end.
     */
    void readAllAsync(
        nx::utils::fs::FileAsyncIoScheduler* scheduler,
        nx::utils::fs::ReadAllHandler handler);

    /**
     * Starts asynchronous write call. On completion handler is invoked.
     * Note: Not all bytes of buffer can be written only in case of an error.
     */
    void writeAsync(
        nx::utils::fs::FileAsyncIoScheduler* scheduler,
        const nx::Buffer& buffer,
        nx::utils::fs::IoCompletionHandler handler);

    /**
     * Starts asynchronous close call. On completion handler is invoked.
     */
    void closeAsync(
        nx::utils::fs::FileAsyncIoScheduler* scheduler,
        nx::utils::fs::CloseHandler handle);

private:
    std::unique_ptr<IQnFile> m_delegate;
};

NX_UTILS_API std::optional<FileStat> stat(const std::string& path);

} // nx::utils::fs

//-------------------------------------------------------------------------------------------------

/**
 * Regular file class.
 * Recommended over QFile due to performance considerations.
 */
class NX_UTILS_API QnFile:
    public IQnFile,
    public std::enable_shared_from_this<QnFile>
{
public:
    QnFile();
    QnFile(const QString& fName);
    QnFile(int fd);
    virtual ~QnFile();
    void setFileName(const QString& fName) { m_fileName = fName; }
    QString fileName() const { return m_fileName; }

    virtual bool open(const QIODevice::OpenMode& mode, unsigned int systemDependentFlags = 0) override;
    virtual void close() override;
    virtual qint64 read(char* buffer, qint64 count) override;
    virtual bool eof() const override;

    /**
     * @return Bytes written or -1 in case of error
     * (use SystemError::getLastOSErrorCode() to get error code).
     */
    virtual qint64 write(const char* buffer, qint64 count) override;

    virtual void sync();
    virtual bool isOpen() const override;
    virtual qint64 size() const override;
    virtual bool seek( qint64 offset) override;
    virtual bool truncate( qint64 newFileSize) override;
    virtual QString url() const override { return fileName(); }

    /**
     * @return true if file system entry with name fileName exists.
     */
    static bool fileExists( const QString& fileName );

    /** If path /etc/smb.conf, returns /etc/, if /etc/ returns /etc/. */
    static QString filePath( const QString& path );

    /** If path /etc/smb.conf, returns smb.conf. */
    static QString fileName( const QString& path );

    /** If path /etc/smb.conf, returns smb. */
    static QString baseName( const QString& path );

    static QString absolutePath( const QString& path );

protected:
    QString m_fileName;

private:
#if defined(_WIN32)
    using Handle = void*;
#else
    using Handle = int;
#endif

    Handle m_fd = 0;
    bool m_eof = false;
};
