#include "user_role_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

UserRoleData::UserRoleData(
    const QnUuid& id,
    const QString& name,
    GlobalPermissions permissions)
    :
    IdData(id),
    name(name),
    permissions(permissions)
{
}

PredefinedRoleData::PredefinedRoleData(
    const QString& name,
    GlobalPermissions permissions,
    bool isOwner)
    :
    name(name),
    permissions(permissions),
    isOwner(isOwner)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (UserRoleData)(PredefinedRoleData),
    (eq)(ubjson)(json)(xml)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
