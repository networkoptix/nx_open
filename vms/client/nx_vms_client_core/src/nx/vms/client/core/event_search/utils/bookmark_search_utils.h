// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::client::core {

class SystemContext;

/** The format depends on the version of the current server of the system the request goes to. */
NX_VMS_CLIENT_CORE_API QString formatFilterText(
    const QString& text, const SystemContext* systemContext);

} // namespace nx::vms::client::core
