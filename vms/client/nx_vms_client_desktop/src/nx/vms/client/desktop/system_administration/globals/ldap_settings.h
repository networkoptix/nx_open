// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/vms/api/data/ldap.h>

namespace nx::vms::client::desktop::ldap_settings {

Q_NAMESPACE
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class TestState
{
    initial,
    connecting,
    ok,
    error,
};
Q_ENUM_NS(TestState)

enum class Sync
{
    disabled = (int) nx::vms::api::LdapSyncMode::disabled,
    groupsOnly = (int) nx::vms::api::LdapSyncMode::groupsOnly,
    usersAndGroups = (int) nx::vms::api::LdapSyncMode::usersAndGroups,
};
Q_ENUM_NS(Sync)

void registerQmlType();

} // namespace nx::vms::client::desktop::ldap_settings
