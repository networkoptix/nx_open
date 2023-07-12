// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/instrument.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API UserExternalId
{
    /**%apidoc Distinguished Name (e.g. LDAP DN). */
    QString dn;

    /**%apidoc Id of synchronization when dn was fetched. */
    QString syncId;

    bool operator==(const UserExternalId& other) const = default;
};
#define UserExternalId_Fields (dn)(syncId)
NX_VMS_API_DECLARE_STRUCT(UserExternalId)
NX_REFLECTION_INSTRUMENT(UserExternalId, UserExternalId_Fields)

void serialize_field(const UserExternalId& from, QVariant* to);
void deserialize_field(const QVariant& from, UserExternalId* to);

/**%apidoc
 * %param[unused] syncId
 */
struct NX_VMS_API UserExternalIdModel: UserExternalId
{
    /**%apidoc[readonly] Is in sync with source server (e.g. LDAP). */
    bool synced = false;

    UserExternalIdModel(UserExternalId base): UserExternalId{std::move(base)} {}
    UserExternalIdModel() = default;
};
#define UserExternalIdModel_Fields UserExternalId_Fields(synced)
QN_FUSION_DECLARE_FUNCTIONS(UserExternalIdModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserExternalIdModel, UserExternalIdModel_Fields)

} // namespace nx::vms::api
