#pragma once

#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <test_support/mediaserver_launcher.h>

namespace nx::vms::server::test::test_support {

vms::api::UserDataEx createUser(
    MediaServerLauncher* server,
    nx::vms::api::GlobalPermission permissions,
    const QString& name,
    const QString& password = "password");

vms::api::UserDataEx getOwner(MediaServerLauncher* server);

} // namespace nx::vms::server::test::test_support
