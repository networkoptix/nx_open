#pragma once

#include <nx/update/common_update_installer.h>
#include "server_module_aware.h"

namespace nx {
namespace vms::server {

class ServerUpdateInstaller: public CommonUpdateInstaller, public ServerModuleAware
{
public:
    ServerUpdateInstaller(QnMediaServerModule* serverModule);
private:
    virtual QString dataDirectoryPath() const override;
    virtual bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const override;
};

} // namespace vms::server
} // namespace nx