#pragma once

#include <functional>
#include <memory>
#include <QtCore>

namespace nx {
namespace update {
namespace installer {
namespace detail {

enum class PrepareResult
{
    ok,
    corruptedArchive,
    noFreeSpace,
    cleanTemporaryFilesError,
    alreadyStarted,
    updateContentsError,
    unknownError,
};

using PrepareUpdateCompletionHandler = std::function<void(PrepareResult /*resultCode*/)>;

class NX_UPDATE_API AbstractUpdates2Installer
{
public:
    virtual ~AbstractUpdates2Installer() = default;
    virtual void prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler) = 0;
    virtual bool install() = 0;
    virtual void stopSync() = 0;
};

} // namespace detail
} // namespace installer
} // namespace update
} // namespace nx
