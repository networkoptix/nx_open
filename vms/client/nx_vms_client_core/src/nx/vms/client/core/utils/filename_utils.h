// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::client::core {

/** True if the filename is a safe leaf name. */
NX_VMS_CLIENT_CORE_API bool isFilenameSafe(const QString& unsafeFilename);

/**
 * Returns `unsafeFilename` unchanged if it is a pure leaf name (no path separators, not "." or
 * "..", not empty, no trailing whitespace or dot); otherwise returns an empty string. Callers
 * are expected to log the rejection themselves so the class/method context is preserved.
 */
NX_VMS_CLIENT_CORE_API QString sanitizeFilename(const QString& unsafeFilename);

} // namespace nx::vms::client::core
