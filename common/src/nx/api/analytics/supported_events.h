#pragma once

#include <QtCore/QList>

#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/**
 * Subset of events, applicable to the given camera.
 */
using AnalyticsSupportedEvents = QList<QnUuid>;

/**
 * If server receives pluginManifest which first event has id kResetPluginManifestEventId,
 * server clears the list of events of this plugin.
 * Such id is necessary for debug and functional testing.
 */
extern const QnUuid kResetPluginManifestEventId;

} // namespace api
} // namespace nx
