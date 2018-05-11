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

namespace nx {
namespace mediaserver {

namespace detail {

/**
 * Helper class for getting unused domain socket path postfix.
 */
class UniqueIdHelper
{
public:
    int take() const;
    void putBack(int id);

private:
    mutable QBitArray m_ids{100};
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_condition;

    int takeFirstAvailable() const;
};

} // namespace detail

/**
 * Helper tool to execute some system actions as root.
 */
class RootTool
{
public:
    RootTool(const QString& toolPath);

    Qn::StorageInitResult mount(const QUrl& url, const QString& path);
    Qn::StorageInitResult remount(const QUrl& url, const QString& path);
    SystemCommands::UnmountCode unmount(const QString& path);
    bool changeOwner(const QString& path);
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
    const QString m_toolPath;
    detail::UniqueIdHelper m_idHelper;

    template<typename R, typename DefaultAction, typename SocketAction>
    R commandHelper(
        R defaultValue, const QString& path, const char* command,
        DefaultAction defaultAction, SocketAction socketAction);

    template<typename DefaultAction>
    qint64 int64SingleArgCommandHelper(
        const QString& path, const char* command, DefaultAction defaultAction);

    template<typename DefaultAction, typename... Args>
    std::string stringCommandHelper(const char* command, DefaultAction action, Args&&... args);

    template<typename Action>
    void execAndReadResult(int socketPostfix, const std::vector<QString>& args, Action action);

    bool waitForProc(int childPid);
    int forkRoolTool(const std::vector<QString>& args);
    bool execAndWait(const std::vector<QString>& args);
};

/** Finds tool next to a appticationPath. */
std::unique_ptr<RootTool> findRootTool(const QString& applicationPath);

} // namespace mediaserver
} // namespace nx
