#include "system_commands.h"
#include <utils/common/util.h>
#include <utils/fs/file.h>
#include <QtCore/QDir>

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

int64_t SystemCommands::freeSpace(const std::string& path)
{
    return getDiskFreeSpace(QString::fromStdString(path));
}

int64_t SystemCommands::totalSpace(const std::string& path)
{
    return getDiskTotalSpace(QString::fromStdString(path));
}

bool SystemCommands::isPathExists(const std::string& path)
{
    return QFileInfo::exists(QString::fromStdString(path));
}

std::string SystemCommands::serializedFileList(const std::string& /*path*/)
{
    return "";
}

int64_t SystemCommands::fileSize(const std::string& path)
{
    return QnFile::getFileSize(QString::fromStdString(path));
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