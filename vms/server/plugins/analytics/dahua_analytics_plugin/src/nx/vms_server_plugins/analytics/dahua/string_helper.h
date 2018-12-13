#pragma once

#include <QtCore/QString>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

QString buildCaption(const EngineManifest& manifest, const Event& event);

QString buildDescription(const EngineManifest& manifest, const Event& event);

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
