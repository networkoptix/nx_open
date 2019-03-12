#pragma once

#include <QtCore/QString>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

QString buildCaption(
    const Hikvision::EngineManifest& manifest,
    const HikvisionEvent& event);

QString buildDescription(
    const Hikvision::EngineManifest& manifest,
    const HikvisionEvent& event);

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
