// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <qglobal.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>
#include "../file.h"

#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

#ifdef Q_OS_DARWIN
#define O_LARGEFILE 0
#endif

#if defined(Q_OS_IOS) || (defined(Q_OS_ANDROID) && __ANDROID_API__ < 21)
#define stat64 stat
#define fstat64 fstat
#endif

int makeUnixOpenFlags(const QIODevice::OpenMode& oflag)
{
    int sysFlags = 0;

    if( oflag & QIODevice::ReadOnly )
        sysFlags = O_RDONLY;

    if( oflag & QIODevice::WriteOnly )
    {
        if( oflag & QIODevice::ReadOnly)
            sysFlags = O_RDWR | O_TRUNC;
        else
            sysFlags = O_WRONLY | O_TRUNC;
        if( !(oflag & QIODevice::Append) )
            sysFlags |= O_CREAT;
        if( !(oflag & QIODevice::Truncate))
        {
            sysFlags &= ~O_TRUNC;
        }
        if( oflag & QIODevice::Append )
        {
            sysFlags |= O_APPEND;
            sysFlags &= ~O_TRUNC;
        }
    }

    return sysFlags;
}

QnFile::QnFile() = default;

QnFile::QnFile(const QString& fName): m_fileName(fName)
{
}

QnFile::QnFile(int fd): m_eof(false)
{
    m_fd = fd;
}

QnFile::~QnFile()
{
    if( isOpen() )
        close();
}

bool QnFile::open(const QIODevice::OpenMode& mode, unsigned int systemDependentFlags)
{
    if (m_fd > 0)
        return true;

    if( isOpen() )
        close();

    int sysFlags = makeUnixOpenFlags(mode);
    int handle = ::open(
        m_fileName.toUtf8(), sysFlags | O_LARGEFILE | systemDependentFlags,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

    if (handle == -1 && (mode & QIODevice::WriteOnly) && errno == ENOENT)
    {
        QDir dir;
        if (dir.mkpath(QnFile::absolutePath(m_fileName)))
            handle = ::open( m_fileName.toUtf8(), sysFlags | O_LARGEFILE | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
    }

    if (handle == -1)
        m_fd = 0;
    else
        m_fd = handle;

    return m_fd != 0;
}

void QnFile::close()
{
    ::close( m_fd );
    m_fd = 0;
}

bool QnFile::eof() const { return m_eof; }

qint64 QnFile::read( char* buffer, qint64 count )
{
    if( !isOpen() )
        return -1;
    qint64 bytesRead = ::read(m_fd, buffer, count);
    if (count > 0 && bytesRead == 0)
        m_eof = true;
    return bytesRead;
}

qint64 QnFile::write(const char* buffer, qint64 count)
{
    auto printLogMessage =
        [this, count, buffer](const QString& m)
        {
            NX_ERROR(
                this, "Write failed. %1 Errno: %2. Buffer: %3 size: %4, fd: %5, fd is valid: %6",
                m, errno, (const void*) buffer, count, m_fd,
                fcntl(m_fd, F_GETFD) == 0);
        };

    if (!isOpen())
    {
        printLogMessage(lit("File is not opened."));
        return -1LL;
    }
    qint64 writtenBytes = ::write( m_fd, buffer, count );
    if (writtenBytes == -1)
        printLogMessage(lit(""));
    else if (writtenBytes < count)
        printLogMessage(lit("Failed to write all bytes."));

    return writtenBytes;
}

bool QnFile::isOpen() const
{
    return m_fd != 0;
}

qint64 QnFile::size() const
{
#if defined(Q_OS_DARWIN)
    struct stat buf;
    if( isOpen() && fstat( m_fd, &buf ) == 0 )
        return buf.st_size;
#else
    struct stat64 buf;
    if( isOpen() && fstat64( m_fd, &buf ) == 0 )
        return buf.st_size;
#endif

    return -1;
}

bool QnFile::seek( qint64 offset)
{
    if( !isOpen() )
        return false;

#if defined(Q_OS_DARWIN) || defined(Q_OS_ANDROID)
    return lseek( m_fd, offset, SEEK_SET) != -1;
#else
    return lseek64( m_fd, offset, SEEK_SET) != -1;
#endif
}

bool QnFile::truncate( qint64 newFileSize )
{
#if defined(Q_OS_DARWIN) || defined(Q_OS_ANDROID)
    return ftruncate( m_fd, newFileSize ) == 0;
#else
    return ftruncate64( m_fd, newFileSize ) == 0;
#endif
}

void QnFile::sync()
{
    ::sync();
}

bool QnFile::fileExists( const QString& fileName )
{
#if defined(Q_OS_DARWIN)
    struct stat fstat;
    int retCode = stat( fileName.toLocal8Bit(), &fstat );
#else
    struct stat64 fstat;
    int retCode = stat64( fileName.toLocal8Bit(), &fstat );
#endif
    return retCode == 0;
}
