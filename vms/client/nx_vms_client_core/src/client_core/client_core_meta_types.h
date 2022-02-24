// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

typedef QSet<QString> QnStringSet;
namespace nx::vms::client::core {

void initializeMetaTypes();

} // namespace nx::vms::client::core
