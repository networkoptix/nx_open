#pragma once

#include <nx/update/common_update_installer.h>
#include "server_module_aware.h"

namespace nx {
namespace vms::server {

class ServerUpdateInstaller: public CommonUpdateInstaller, public ServerModuleAware
{
public:
    ServerUpdateInstaller(QnMediaServerModule* serverModule);
    virtual QString dataDirectoryPath() const override;
    virtual QString component() const override;
    virtual int64_t freeSpace(const QString& path) const override;

private:
    virtual bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const override;
};

} // namespace vms::server
} // namespace nx
