#pragma once

#include <functional>
#include <memory>
#include <QtCore>

namespace nx {
namespace update {

enum class PrepareResult
{
    ok,
    corruptedArchive,
    noFreeSpace,
    unknownError
};

using PrepareUpdateCompletionHandler =
    std::function<void(PrepareResult /*resultCode*/, const QString& /*updateId*/)>;

class NX_UPDATE_API AbstractUpdates2Installer
{
public:
    virtual ~AbstractUpdates2Installer() = default;
    virtual void prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler) = 0;
    virtual void install(const QString& updateId) = 0;
};

using AbstractUpdates2InstallerPtr = std::shared_ptr<AbstractUpdates2Installer>;

} // namespace update
} // namespace nx
