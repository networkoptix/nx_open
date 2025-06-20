// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/vms/api/data/resource_property_key.h>

namespace nx::analytics {

static const QString kPluginDescriptorsProperty("pluginDescriptors");
static const QString kEngineDescriptorsProperty("engineDescriptors");
static const QString kGroupDescriptorsProperty("groupDescriptors");
static const QString kEventTypeDescriptorsProperty("eventTypeDescriptors");
static const QString kObjectTypeDescriptorsProperty("objectTypeDescriptors");
static const QString kEnumTypeDescriptorsProperty("enumTypeDescriptors");
static const QString kColorTypeDescriptorsProperty("colorTypeDescriptors");

static const QString kDescriptorsProperty(
    nx::vms::api::server_properties::detail::kAnalyticsTaxonomyDescriptorsValue);

} // namespace nx::analytics
