// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQml/QtQml>

#include "ldap_settings.h"

namespace nx::vms::client::desktop::ldap_settings {

void registerQmlType()
{
    // Export TestState enum to QML.
    qmlRegisterUncreatableMetaObject(staticMetaObject,
        "nx.vms.client.desktop", 1, 0, "LdapSettings",
        "Cannot create an instance of LdapSettings.");
}

} // namespace nx::vms::client::desktop::ldap_settings
