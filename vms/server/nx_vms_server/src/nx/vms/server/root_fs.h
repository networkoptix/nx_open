#pragma once

#include <memory>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <common/common_globals.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/system_commands.h>
#include <core/resource/abstract_storage_resource.h>

class QnMediaServerModule;

namespace nx {
namespace vms::server {

/**
 * Helper class to execute some system actions as root.
 */
class RootFileSystem
{
public:
    RootFileSystem(bool useTool);

    Qn::StorageInitResult mount(const QUrl& url, const QString& path);
    Qn::StorageInitResult remount(const QUrl& url, const QString& path);
    SystemCommands::UnmountCode unmount(const QString& path);
    bool changeOwner(const QString& path, bool isRecursive = true);
    bool makeDirectory(const QString& path);
    bool removePath(const QString& path);
    bool rename(const QString& oldPath, const QString& newPath);
    int open(const QString& path, QIODevice::OpenMode mode);
    qint64 freeSpace(const QString& path);
    qint64 totalSpace(const QString& path);
    bool isPathExists(const QString& path);
    QnAbstractStorageResource::FileInfoList fileList(const QString& path);
    qint64 fileSize(const QString& path);
    QString devicePath(const QString& fsPath);
    bool dmiInfo(QString* outPartNumber, QString *outSerialNumber);

private:
    const bool m_ignoreTool;
};

/** Finds tool next to a appticationPath. */
std::unique_ptr<RootFileSystem> instantiateRootFileSystem(
    bool isRootToolEnabled,
    const QString& applicationPath);

} // namespace vms::server
} // namespace nx
