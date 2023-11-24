// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::utils {

/**
 * Try to register system nov files association.
 * @returns true if the association registered successfully (or was already registered),
 *     false otherwise.
 */
bool registerNovFilesAssociation(
    const QString& customizationId,
    const QString& applicationLauncherBinaryPath,
    const QString& applicationBinaryName,
    const QString& applicationName);

} // namespace nx::vms::utils
