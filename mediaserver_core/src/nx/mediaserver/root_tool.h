#pragma once

#include <memory>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

namespace nx {
namespace mediaserver {

/**
 * Helper tool to execute some system actions as root.
 */
class RootTool
{
public:
    RootTool(const QString& toolPath);

    bool mount(const QUrl& url, const QString& path);
    bool remount(const QUrl& url, const QString& path);
    bool unmount(const QString& path);

    bool changeOwner(const QString& path);
    bool touchFile(const QString& path);
    bool makeDirectory(const QString& path);

private:
    bool execute(const std::vector<QString>& args);

private:
    const QString m_toolPath;
};

/** Finds tool next to a appticationPath. */
std::unique_ptr<RootTool> findRootTool(const QString& applicationPath);

} // namespace mediaserver
} // namespace nx
