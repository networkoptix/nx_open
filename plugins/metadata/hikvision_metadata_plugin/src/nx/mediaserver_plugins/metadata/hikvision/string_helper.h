#pragma once

#include <QtCore/QString>
#include <QtCore/QFlags>

#include <unordered_map>

#include <boost/optional/optional.hpp>

#include "common.h"
#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

QString buildCaption(
    const Hikvision::DriverManifest& manifest,
    const HikvisionEvent& event);

QString buildDescription(
    const Hikvision::DriverManifest& manifest,
    const HikvisionEvent& event);

} // namespace hikvision
} // namespace nx
} // namespace mediaserver
} // namespace plugins
