#pragma once

#include <QtCore/QString>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop::layout {

/**
 * Reads layout from file. Does not add it to resource pool.
 * layoutUrl should be using native path separators.
 */
QnLayoutResourcePtr layoutFromFile(const QString& layoutUrl);

} // namespace nx::vms::client::desktop::layout