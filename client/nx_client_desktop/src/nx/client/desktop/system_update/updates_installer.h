#pragma once

#ifdef TO_BE_REMOVED
#include <nx/update/installer/common_updates2_installer.h>

namespace nx {
namespace client {
namespace desktop {

class UpdatesInstaller: public update::CommonUpdates2Installer
{
private:
    virtual QString dataDirectoryPath() const override;
    virtual bool initializeUpdateLog(
        const QString& targetVersion,
        QString* logFileName) const override;
};

} // namespace desktop
} // namespace client
} // namespace nx
#endif
