#pragma once

#include <QtCore/QList>

#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/**
 * Subset of events, applicable to the given camera.
 */
using AnalyticsSupportedEvents = QList<QString>;

} // namespace api
} // namespace nx
