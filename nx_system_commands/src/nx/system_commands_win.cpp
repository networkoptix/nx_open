#include "system_commands.h"
#include <utils/common/util.h>
#include <utils/fs/file.h>
#include <QtCore/QDir>

namespace nx {

SystemCommands::MountCode SystemCommands::mount(
    const std::string& /*url*/,
    const std::string& /*directory*/,
    const boost::optional<std::string>& /*username*/,
    const boost::optional<std::string>& /*password*/,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return MountCode::otherError;
}

SystemCommands::UnmountCode SystemCommands::unmount(
    const std::string& /*directory*/,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return UnmountCode::noPermissions;
}

bool SystemCommands::changeOwner(const std::string& /*path*/, bool /*isRecursive*/)
{
    return true;
}

bool SystemCommands::makeDirectory(const std::string& directoryPath)
{
    return QDir().mkpath(QString::fromStdString(directoryPath));
}

bool SystemCommands::install(const std::string& /*debPackage*/)
{
    return false;
}

void SystemCommands::showIds()
{
}

bool SystemCommands::setupIds()
{
    return false;
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
    return QDir(QString::fromStdString(path)).removeRecursively();
}

int SystemCommands::open(
    const std::string& /*path*/,
    int /*mode*/,
    bool /*usePipe*/,
    int /*socketPostfix*/)
{
    return -1;
}

bool SystemCommands::rename(const std::string& oldPath, const std::string& newPath)
{
    return MoveFileW(
        (LPCTSTR) QString::fromStdString(oldPath).constData(),
        (LPCTSTR) QString::fromStdString(newPath).constData());
}

int64_t SystemCommands::freeSpace(
    const std::string& path,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return getDiskFreeSpace(QString::fromStdString(path));
}

int64_t SystemCommands::totalSpace(
    const std::string& path,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return getDiskTotalSpace(QString::fromStdString(path));
}

bool SystemCommands::isPathExists(
    const std::string& path,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return QFileInfo::exists(QString::fromStdString(path));
}

std::string SystemCommands::serializedFileList(
    const std::string& /*path*/,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return "";
}

int64_t SystemCommands::fileSize(
    const std::string& path,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return QnFile::getFileSize(QString::fromStdString(path));
}

std::string SystemCommands::devicePath(
    const std::string& path,
    bool /*reportViaSocket*/,
    int /*socketPostfix*/)
{
    return path;
}

std::string SystemCommands::serializedDmiInfo(bool /*reportViaSocket*/, int /*socketPostfix*/)
{
    return "";
}

} // namespace nx