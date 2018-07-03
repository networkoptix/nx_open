#pragma once

#include <nx/update/installer/common_updates2_installer.h>

namespace nx {
namespace client {
namespace desktop {
namespace updates2 {

class ClientUpdates2Installer: public update::CommonUpdates2Installer
{
private:
    virtual QString dataDirectoryPath() const override;
    virtual bool initializeUpdateLog(
        const QString& targetVersion,
        QString* logFileName) const override;
};

} // namespace updates2
} // namespace desktop
} // namespace client
} // namespace nx
