// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QString>

typedef QSet<QString> QnStringSet;
namespace nx::vms::client::core {

NX_VMS_CLIENT_CORE_API void initializeMetaTypes();

} // namespace nx::vms::client::core
