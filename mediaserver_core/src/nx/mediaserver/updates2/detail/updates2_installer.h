#pragma once

#include <nx/mediaserver/updates2/detail/abstract_updates2_installer.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

class Updates2Installer: public AbstractUpdates2Installer
{
public:
    virtual void prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler) override;
    virtual void install(const QString& updateId) override;
};

} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
