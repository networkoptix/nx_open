// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include "async_file_processor.h"

static void splitPath( const QString& path, QString* dirPath, QString* fileName )
{
#ifdef _WIN32
    //on win32, '/' and '\' is allowed as path separator
    const int slashPos = path.lastIndexOf(QLatin1Char('/'));
    const int reverseSlashPos = path.lastIndexOf(QLatin1Char('\\'));
    const int separatorPos =
        slashPos == -1
        ? reverseSlashPos
        : (reverseSlashPos == -1 ? slashPos : std::max<int>(reverseSlashPos, slashPos));
#else
    const int separatorPos = path.lastIndexOf(QLatin1Char('/'));
#endif

    if( separatorPos == -1 )
    {
        if( dirPath )  *dirPath = QString();
        if( fileName ) *fileName = path;
    }
    else
    {
        if( dirPath )  *dirPath = path.mid(0,separatorPos+1);  //including separator with path
        if( fileName ) *fileName = path.mid(separatorPos+1);
    }
}

std::optional<nx::Buffer> IQnFile::readAll()
{
    static constexpr int kReadSize = 16 * 1024;

    nx::Buffer buf;
    while (!eof())
    {
        const auto curPos = buf.size();
        buf.resize(buf.size() + kReadSize);
        const auto bytesRead = read(buf.data() + curPos, buf.size() - curPos);
        if (bytesRead < 0)
            return std::nullopt;

        buf.resize((int) (curPos + bytesRead));
    }

    return buf;
}

//-------------------------------------------------------------------------------------------------

QString QnFile::filePath( const QString& path )
{
    QString filePath;
    splitPath( path, &filePath, NULL );
    return filePath;
}

QString QnFile::fileName( const QString& path )
{
    QString _fileName;
    splitPath( path, NULL, &_fileName );
    return _fileName;
}

QString QnFile::baseName( const QString& path )
{
    const QString _fileName = fileName( path );
    const int sepPos = _fileName.indexOf(QLatin1Char('.'));
    return sepPos == -1 ? _fileName : _fileName.mid(0, sepPos);
}

QString QnFile::absolutePath( const QString& path )
{
    return QFileInfo(path).absolutePath();
}

//-------------------------------------------------------------------------------------------------

namespace nx::utils::fs {

namespace {

class QtFileWrapper:
    public IQnFile
{
public:
    QtFileWrapper(const QString& filename);

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
private:
    QFile m_file;
};

QtFileWrapper::QtFileWrapper(const QString& filename):
    m_file(filename)
{
}

bool QtFileWrapper::open(
    const QIODevice::OpenMode& mode,
    unsigned int /*systemDependentFlags*/)
{
    return m_file.open(mode);
}

void QtFileWrapper::close()
{
    m_file.close();
}

qint64 QtFileWrapper::read(char* buffer, qint64 count)
{
    return m_file.read(buffer, count);
}

qint64 QtFileWrapper::write(const char* buffer, qint64 count)
{
    return m_file.write(buffer, count);
}

bool QtFileWrapper::isOpen() const
{
    return m_file.isOpen();
}

qint64 QtFileWrapper::size() const
{
    return m_file.size();
}

bool QtFileWrapper::seek(qint64 offset)
{
    return m_file.seek(offset);
}

bool QtFileWrapper::truncate(qint64 newFileSize)
{
    return m_file.resize(newFileSize);
}

bool QtFileWrapper::eof() const
{
    return m_file.atEnd();
}

QString QtFileWrapper::url() const
{
    return m_file.fileName();
}

//-------------------------------------------------------------------------------------------------

std::optional<FileStat> statQtResourceFile(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists())
    {
        SystemError::setLastErrorCode(SystemError::fileNotFound);
        return std::nullopt;
    }

    FileStat result;
    memset(&result, 0, sizeof(result));
    result.st_size = info.size();

    if (info.isFile())
        result.st_mode |= S_IFREG;
    if (info.isDir())
        result.st_mode |= S_IFDIR;
    result.st_uid = info.ownerId();

    // TODO: permissions.

    return result;
}

std::optional<FileStat> statRegularFile(const std::string& path)
{
    FileStat info;
    int result = 0;

#ifdef _WIN32
    result = _stat64(path.c_str(), &info);
#elif defined(__APPLE__)
    result = stat(path.c_str(), &info);
#else
    result = stat64(path.c_str(), &info);
#endif

    if (result == 0)
        return info;

#if defined(_WIN32)
    SystemError::setLastErrorCode(SystemError::fileNotFound);
#endif

    return std::nullopt;
}

} // namespace

//-------------------------------------------------------------------------------------------------

File::File(const std::string& filename)
{
    if (!filename.empty() && filename[0] == ':')
        m_delegate = std::make_unique<QtFileWrapper>(filename.c_str());
    else
        m_delegate = std::make_unique<QnFile>(filename.c_str());
}

bool File::open(
    const QIODevice::OpenMode& mode,
    unsigned int systemDependentFlags)
{
    return m_delegate->open(mode, systemDependentFlags);
}

void File::close()
{
    return m_delegate->close();
}

qint64 File::read(char* buffer, qint64 count)
{
    return m_delegate->read(buffer, count);
}

qint64 File::write(const char* buffer, qint64 count)
{
    return m_delegate->write(buffer, count);
}

bool File::isOpen() const
{
    return m_delegate->isOpen();
}

qint64 File::size() const
{
    return m_delegate->size();
}

bool File::seek(qint64 offset)
{
    return m_delegate->seek(offset);
}

bool File::truncate(qint64 newFileSize)
{
    return m_delegate->truncate(newFileSize);
}

bool File::eof() const
{
    return m_delegate->eof();
}

QString File::url() const
{
    return m_delegate->url();
}

void File::openAsync(
    nx::utils::fs::FileAsyncIoScheduler* scheduler,
    const QIODevice::OpenMode& mode,
    unsigned int systemDependentFlags,
    nx::utils::fs::OpenHandler handler)
{
    scheduler->open(this, mode, systemDependentFlags, std::move(handler));
}

void File::readAsync(
    nx::utils::fs::FileAsyncIoScheduler* scheduler,
    nx::Buffer* buf,
    nx::utils::fs::IoCompletionHandler handler)
{
    scheduler->read(this, buf, std::move(handler));
}

void File::readAllAsync(
    nx::utils::fs::FileAsyncIoScheduler* scheduler,
    nx::utils::fs::ReadAllHandler handler)
{
    scheduler->readAll(this, std::move(handler));
}

void File::writeAsync(
    nx::utils::fs::FileAsyncIoScheduler* scheduler,
    const nx::Buffer& buffer,
    nx::utils::fs::IoCompletionHandler handler)
{
    scheduler->write(this, buffer, std::move(handler));
}

void File::closeAsync(
    nx::utils::fs::FileAsyncIoScheduler* scheduler,
    nx::utils::fs::CloseHandler handler)
{
    scheduler->close(this, std::move(handler));
}

//-------------------------------------------------------------------------------------------------

std::optional<FileStat> stat(const std::string& path)
{
    if (!path.empty() && path[0] == ':')
        return statQtResourceFile(path.c_str());
    else
        return statRegularFile(path);
}

} // namespace nx::utils::fs
