// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

namespace nx::vms::event {

NX_VMS_COMMON_API QStringList splitOnPureKeywords(const QString& keywords);
NX_VMS_COMMON_API bool checkForKeywords(const QString& value, const QStringList& keywords);

} // namespace nx::vms::event
