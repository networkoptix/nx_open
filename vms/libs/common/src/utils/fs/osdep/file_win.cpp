/***********************************************************************
* File: file_win32.cpp
* Author: Andrey Kolesnikov
* Date: 5 dec 2006
***********************************************************************/

#include "../file.h"

#ifdef Q_OS_WIN

#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <sstream>
#include <io.h>
#include <windows.h>
#include <sys/stat.h>
#include <nx/utils/log/log.h>

QnFile::QnFile(): m_impl(INVALID_HANDLE_VALUE), m_eof(false)
{

}

QnFile::QnFile(const QString& fName): m_fileName(fName),
                                      m_impl(INVALID_HANDLE_VALUE),
                                      m_eof(false)
{

}

QnFile::QnFile(int /*fd*/): m_eof(false)
{
    NX_ASSERT(false, "Windows is not supported");
}

QnFile::~QnFile()
{
    if( isOpen() )
        close();
}

bool QnFile::open(const QIODevice::OpenMode& openMode, unsigned int systemDependentFlags)
{
    // All files are opened in share mode (both read and write).
    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    int accessRights = 0;
    if (openMode & QIODevice::ReadOnly)
        accessRights |= GENERIC_READ;
    if (openMode & QIODevice::WriteOnly) {
        accessRights |= GENERIC_WRITE;
        if (!(openMode & QIODevice::ReadOnly))
            shareMode = FILE_SHARE_READ;
    }

    SECURITY_ATTRIBUTES securityAtts = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };

    // WriteOnly can create files, ReadOnly cannot.
    DWORD creationDisp = (openMode & QIODevice::WriteOnly) ? OPEN_ALWAYS : OPEN_EXISTING;

    if (systemDependentFlags == 0)
        systemDependentFlags = FILE_ATTRIBUTE_NORMAL;

    // Create the file handle.
    m_impl = CreateFile((const wchar_t*)m_fileName.constData(),
        accessRights,
        shareMode,
        &securityAtts,
        creationDisp,
        (DWORD) systemDependentFlags,
        NULL);

    if (m_impl == INVALID_HANDLE_VALUE && (openMode & QIODevice::WriteOnly) && GetLastError() == ERROR_PATH_NOT_FOUND)
    {
        QDir dir;
        if (dir.mkpath(QnFile::absolutePath(m_fileName)))
        {
            m_impl = CreateFile((const wchar_t*)m_fileName.constData(),
                accessRights,
                shareMode,
                &securityAtts,
                creationDisp,
                (DWORD) systemDependentFlags,
                NULL);
        }
    }

    // Bail out on error.
    if (m_impl == INVALID_HANDLE_VALUE) {
        NX_WARNING(this, lm("Failed to open file %1, mode is %2, error is %3")
            .args(m_fileName, openMode, qt_error_string()));
        return false;
    }

    // Truncate the file after successfully opening it if Truncate is passed.
    if (openMode & QIODevice::Truncate)
        truncate(0);

    return true;
}

bool QnFile::eof() const
{
    return m_eof;
}

void QnFile::close()
{
    //sync();
    CloseHandle( m_impl );
    m_impl = INVALID_HANDLE_VALUE;
}

qint64 QnFile::read(char* buffer, qint64 count )
{
    if( !isOpen() )
        return -1;

    DWORD bytesRead = 0;
    BOOL res = ReadFile( m_impl, buffer, count, &bytesRead, NULL );
    m_eof = res && !bytesRead;
    if( !res )
        return -1;
    return bytesRead;
}

qint64 QnFile::write( const char* buffer, qint64 count )
{
    if( !isOpen() )
        return -1;

    DWORD bytesWritten = 0;
    BOOL res = WriteFile( m_impl, buffer, count, &bytesWritten, NULL );
    if( !res )
        return -1;
    return bytesWritten;
}

void QnFile::sync()
{
    FlushFileBuffers( m_impl );
}

bool QnFile::isOpen() const
{
    return m_impl != INVALID_HANDLE_VALUE;
}

qint64 QnFile::size() const
{
    QnFile* nonConstThis = const_cast<QnFile*>(this);
    nonConstThis->sync();
    DWORD highDw;
    DWORD lowDw = GetFileSize( m_impl, &highDw );
    if( (lowDw == INVALID_FILE_SIZE) && (GetLastError() != NO_ERROR) )
        return -1;
    return ((qint64)highDw << 32) + lowDw;
}

bool QnFile::seek( qint64 offset)
{
    if( !isOpen() )
        return false;

    LONG distanceToMoveLow = (quint32)(offset & 0xffffffff);
    LONG distanceToMoveHigh = (quint32)((offset & 0xffffffff00000000ull) >> 32);

    DWORD newPointerLow = SetFilePointer( m_impl, distanceToMoveLow, &distanceToMoveHigh, FILE_BEGIN );
    if( newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR )
        return false;
    return true;
}

bool QnFile::truncate( qint64 newFileSize)
{
    LONG distanceToMoveLow = (quint32)(newFileSize & 0xffffffff);
    LONG distanceToMoveHigh = (quint32)(newFileSize >> 32);
    DWORD newPointerLow = SetFilePointer(
        m_impl,
        distanceToMoveLow,
        &distanceToMoveHigh,
        FILE_BEGIN );
    int errCode = GetLastError();
    if( (newPointerLow == INVALID_SET_FILE_POINTER) && (errCode != NO_ERROR) )
    {
        qWarning() << "Can't truncate file " << m_fileName << "to size" << newFileSize;
        return false;
    }
    return SetEndOfFile( m_impl ) > 0;
}

bool QnFile::fileExists( const QString& fileName )
{
    struct _stat64 fstat;
    int retCode = _wstat64( (wchar_t*)fileName.utf16(), &fstat );
    return retCode == 0;
}

#endif
