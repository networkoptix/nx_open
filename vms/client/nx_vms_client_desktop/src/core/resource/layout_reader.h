// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <nx/vms/client/core/resource/resource_fwd.h>

namespace nx::vms::client::desktop::layout {

/**
 * Reads layout from file. Does not add it to resource pool.
 * layoutUrl should be using native path separators.
 */
QnFileLayoutResourcePtr layoutFromFile(const QString& layoutUrl, const QString& password = QString());

/** Reloads layout file from disk. Used mainly for setting or resetting the password. */
bool reloadFromFile(QnFileLayoutResourcePtr layout, const QString& password = QString());

} // namespace nx::vms::client::desktop::layout
