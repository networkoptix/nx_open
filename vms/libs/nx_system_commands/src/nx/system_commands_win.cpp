#include "system_commands.h"
#include <QtCore/QDir>
#include <QtCore/QString>
#include <windows.h>

namespace nx {

SystemCommands::MountCode SystemCommands::mount(const std::string& /*url*/,
    const std::string& /*directory*/, const boost::optional<std::string>& /*username*/,
    const boost::optional<std::string>& /*password*/)
{
    return MountCode::otherError;
}

SystemCommands::UnmountCode SystemCommands::unmount(const std::string& /*directory*/)
{
    return UnmountCode::noPermissions;
}

bool SystemCommands::changeOwner(const std::string& /*path*/, int uid, int gid, bool /*isRecursive*/)
{
    return true;
}

bool SystemCommands::makeDirectory(const std::string& directoryPath)
{
    return QDir().mkpath(QString::fromStdString(directoryPath));
}

std::string SystemCommands::lastError() const
{
    return "";
}

bool SystemCommands::checkMountPermissions(const std::string& /*directory*/)
{
    return true;
}

bool SystemCommands::checkOwnerPermissions(const std::string& /*path*/)
{
    return true;
}

bool SystemCommands::execute(
    const std::string& /*command*/,
    std::function<void(const char*)> /*outputAction*/)
{
    return false;
}

bool SystemCommands::removePath(const std::string& path)
{
    QString qpath = QString::fromStdString(path);
    auto fileInfo = QFileInfo(qpath);

    if (fileInfo.isDir())
        return QDir(qpath).removeRecursively();

    return QFile(qpath).remove();
}

int SystemCommands::open(const std::string& /*path*/, int /*mode*/)
{
    return -1;
}

bool SystemCommands::rename(const std::string& oldPath, const std::string& newPath)
{
    return MoveFileW(
        (LPCTSTR) QString::fromStdString(oldPath).constData(),
        (LPCTSTR) QString::fromStdString(newPath).constData());
}

namespace {

bool isLocalPath(const QString& folder)
{
    return folder.length() >= 2 && folder[1] == L':';
}

QString getParentFolder(const QString& root)
{
    QString newRoot = QDir::toNativeSeparators(root);
    if (newRoot.endsWith(QDir::separator()))
        newRoot.chop(1);
    return newRoot.left(newRoot.lastIndexOf(QDir::separator()) + 1);
}

int64_t freeSpaceImpl(const QString& root)
{
    quint64 freeBytesAvailableToCaller = -1;
    quint64 totalNumberOfBytes = -1;
    quint64 totalNumberOfFreeBytes = -1;
    BOOL status = GetDiskFreeSpaceEx(
        (LPCWSTR)root.data(), // pointer to the directory name
        (PULARGE_INTEGER)&freeBytesAvailableToCaller, // receives the number of bytes on disk available to the caller
        (PULARGE_INTEGER)&totalNumberOfBytes, // receives the number of bytes on disk
        (PULARGE_INTEGER)&totalNumberOfFreeBytes // receives the free bytes on disk
    );
    if (!status && isLocalPath(root)) {
        QString newRoot = getParentFolder(root);
        if (!newRoot.isEmpty())
            return freeSpaceImpl(newRoot); // try parent folder
    }
    return freeBytesAvailableToCaller;
}

int64_t totalSpaceImpl(const QString& root)
{
    quint64 freeBytesAvailableToCaller = -1;
    quint64 totalNumberOfBytes = -1;
    quint64 totalNumberOfFreeBytes = -1;
    BOOL status = GetDiskFreeSpaceEx(
        (LPCWSTR)root.data(), // pointer to the directory name
        (PULARGE_INTEGER)&freeBytesAvailableToCaller, // receives the number of bytes on disk available to the caller
        (PULARGE_INTEGER)&totalNumberOfBytes, // receives the number of bytes on disk
        (PULARGE_INTEGER)&totalNumberOfFreeBytes // receives the free bytes on disk
    );
    if (!status && isLocalPath(root)) {
        QString newRoot = getParentFolder(root);
        if (!newRoot.isEmpty())
            return totalSpaceImpl(newRoot); // try parent folder
    }
    return totalNumberOfBytes;
}

} // namespace

int64_t SystemCommands::freeSpace(const std::string& path)
{
    return freeSpaceImpl(QString::fromStdString(path));
}

int64_t SystemCommands::totalSpace(const std::string& path)
{
    return totalSpaceImpl(QString::fromStdString(path));
}

bool SystemCommands::isPathExists(const std::string& path)
{
    return stat(path).exists;
}

SystemCommands::Stats SystemCommands::stat(const std::string& path)
{
    Stats result;
    const auto fileInfo = QFileInfo::exists(QString::fromStdString(path));
    result.exists = fileInfo.exists();

    if (result.exists)
    {
        if (fileInfo.isDir())
            result.type = Stats::FileType::directory;
        else if (fileInfo.isFile())
            result.type = Stats::FileType::regular;
        else
            result.type = Stats::FileType::other;

        result.size = fileInfo.size();
    }

    return result;
}

std::string SystemCommands::serializedFileList(const std::string& /*path*/)
{
    return "";
}

int64_t SystemCommands::fileSize(const std::string& path)
{
    return stat(path).size;
}

std::string SystemCommands::devicePath(const std::string& path)
{
    return path;
}

std::string SystemCommands::serializedDmiInfo()
{
    return "";
}

} // namespace nx