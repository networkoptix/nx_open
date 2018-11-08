#pragma once

#include "id_data.h"

#include <nx/vms/api/types/access_rights_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UserRoleData: IdData
{
    UserRoleData() = default;
    UserRoleData(const QnUuid& id, const QString& name, GlobalPermissions permissions);

    bool isNull() const { return id.isNull(); }

    QString name;
    GlobalPermissions permissions;
};
#define UserRoleData_Fields \
    IdData_Fields \
    (name) \
    (permissions)

/* Struct is not inherited from ApiUserRoleData as it has no 'id' field. */
struct NX_VMS_API PredefinedRoleData: Data
{
    PredefinedRoleData() = default;
    PredefinedRoleData(const QString& name, GlobalPermissions permissions, bool isOwner = false);

    QString name;
    GlobalPermissions permissions;
    bool isOwner = false;
};
#define PredefinedRoleData_Fields \
    (name) \
    (permissions) \
    (isOwner)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::UserRoleData)
Q_DECLARE_METATYPE(nx::vms::api::UserRoleDataList)
Q_DECLARE_METATYPE(nx::vms::api::PredefinedRoleData)
Q_DECLARE_METATYPE(nx::vms::api::PredefinedRoleDataList)
