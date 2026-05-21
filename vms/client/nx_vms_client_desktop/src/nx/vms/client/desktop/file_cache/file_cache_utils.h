// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>
#include <QtCore/QString>

namespace nx::vms::client::desktop::file_cache {

/** True if the filename is a safe leaf name. */
bool isFilenameSafe(const QString& unsafeFilename);

/**
 * Returns `unsafeFilename` unchanged if it is a pure leaf name (no path separators, not "." or
 * "..", not empty, no trailing whitespace or dot); otherwise returns an empty string. Callers
 * are expected to log the rejection themselves so the class/method context is preserved.
 */
QString sanitizeFilename(const QString& unsafeFilename);

/**
 * Stable hash-derived filename for an arbitrary source path.
 * The returned name is a pure leaf name that can be safely used in the cache directory.
 */
QString cachedImageFilename(const QString& sourcePath);

/** True if the filename's extension is allowed for image files. */
bool hasAllowedImageExtension(const QString& filename);

/** Remove the entire local cache directory tree under `<AppLocalData>/cache`. */
void clearLocalCache();

/** Maximum image dimension the renderer can upload as a single texture. */
QSize maxBackgroundImageSize();

} // namespace nx::vms::client::desktop::file_cache
