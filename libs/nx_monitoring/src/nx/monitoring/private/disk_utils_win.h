// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <functional>

#include <windows.h>

#include <QtCore/QStringList>

namespace nx::monitoring {

using WinHandlePtr = std::unique_ptr<HANDLE, std::function<void(HANDLE*)>>;

QStringList getDriveNames();
WinHandlePtr getDriveHandle(const QString& driveName);

} // namespace nx::monitoring
