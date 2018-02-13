#pragma once

#include <nx/update/installer/abstract_updates2_installer.h>

namespace nx {
namespace update {

class NX_UPDATE_API Updates2Installer: public AbstractUpdates2Installer
{
public:
    virtual void prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler) override;
    virtual void install(const QString& updateId) override;
};

} // namespace updates2
} // namespace nx
