#pragma once

#include "server_updates2_installer.h"
#include <media_server/media_server_module.h>
#include <media_server/settings.h>

namespace nx {
namespace mediaserver {
namespace updates2 {

QString ServerUpdates2Installer::dataDirectoryPath() const
{
    return qnServerModule->settings()->getDataDirectory();
}

} // namespace updates2
} // namespace mediaserver
} // namespace nx
