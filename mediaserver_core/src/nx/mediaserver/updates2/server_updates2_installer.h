#pragma once

#include <nx/update/installer/common_updates2_installer.h>

namespace nx {
namespace mediaserver {
namespace updates2 {

class ServerUpdates2Installer: public update::CommonUpdates2Installer
{
private:
    virtual QString dataDirectoryPath() const override;
};

} // namespace updates2
} // namespace mediaserver
} // namespace nx
