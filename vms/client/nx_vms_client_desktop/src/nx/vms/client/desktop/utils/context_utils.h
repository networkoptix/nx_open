// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QObject;

namespace nx::vms::client::desktop {

class WindowContext;

namespace utils {

WindowContext* windowContextFromObject(QObject* parent);

} // namespace utils
} // namespace nx::vms::client::desktop
