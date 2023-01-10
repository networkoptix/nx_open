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

    /**%apidoc Is in sync with source server (e.g. LDAP). */
    bool synced = false;

    UserExternalId() {}
    UserExternalId(QString dn): dn(std::move(dn)), synced(true) {}

    UserExternalId(const UserExternalId&) = default;
    UserExternalId& operator=(const UserExternalId&) = default;
    UserExternalId(UserExternalId&&) = default;
    UserExternalId& operator=(UserExternalId&&) = default;

    bool isEmpty() const { return dn.isEmpty() && !synced; }
    bool operator==(const UserExternalId& other) const = default;
};
#define UserExternalId_Fields (dn)(synced)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UserExternalId)
NX_REFLECTION_INSTRUMENT(UserExternalId, UserExternalId_Fields)

void serialize_field(const UserExternalId& from, QVariant* to);
void deserialize_field(const QVariant& from, UserExternalId* to);

} // namespace nx::vms::api

