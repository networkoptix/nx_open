#pragma once

#include <QtCore/QString>

#include "common.h"

namespace nx::vms_server_plugins::analytics::dahua {

QString buildCaption(const EngineManifest& manifest, const Event& event);

QString buildDescription(const EngineManifest& manifest, const Event& event);

} // namespace nx::vms_server_plugins::analytics::dahua
