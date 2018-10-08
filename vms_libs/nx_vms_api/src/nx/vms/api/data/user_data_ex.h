#pragma once

#include "resource_data.h"

#include <QtCore/QString>

#include <nx/vms/api/types/access_rights_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UserDataEx
{
    QnUuid id;
    QString name;
    GlobalPermissions permissions;
    QString email;
    QString password;
    bool isCloud = false;
    bool isLdap = false;
    bool isEnabled = true;
    QnUuid userRoleId;
    QString fullName;
};

#define UserDataEx_Fields \
    (id) \
    (name) \
    (permissions) \
    (email) \
    (password) \
    (isCloud) \
    (isLdap) \
    (isEnabled) \
    (userRoleId) \
    (fullName)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::UserDataEx)
