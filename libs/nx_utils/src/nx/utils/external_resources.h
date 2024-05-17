// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>
#include <QtCore/QString>

namespace nx::utils {

NX_UTILS_API QDir externalResourcesDirectory();

NX_UTILS_API bool registerExternalResource(
    const QString& filename, const QString& mapRoot = QString());

NX_UTILS_API bool unregisterExternalResource(
    const QString& filename, const QString& mapRoot = QString());

/**
 * Registers all files in the given directory.
 */
NX_UTILS_API bool registerExternalResourceDirectory(const QString& directory);

} // namespace nx::utils
