#pragma once

#include <QtCore/QString>
#include <QtCore/QFlags>

#include <unordered_map>

#include "common.h"
#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

QString buildCaption(const Dahua::EngineManifest& manifest, const DahuaEvent& event);

QString buildDescription(const Dahua::EngineManifest& manifest, const DahuaEvent& event);

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
