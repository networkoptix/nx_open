// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QString>

namespace nx::vms::rules {

// TODO: #amalov This method should reside in the VMS Rules Engine, but the code doing actual
// encryption has no access to engine or system context.
const QSet<QString>& encryptedActionBuilderProperties(const QString& type);

} // namespace nx::vms::rules
