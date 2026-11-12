// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_appearance_provider.h"

#include <nx/vms/client/core/analytics/object_display_settings.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core::analytics {

ObjectAppearanceProvider::ObjectAppearanceProvider(SystemContext* systemContext):
    m_objectTypeDictionary(
        systemContext ? systemContext->analyticsTaxonomyStateWatcher() : nullptr)
{
}

QColor ObjectAppearanceProvider::color(const nx::common::metadata::ObjectMetadata& object) const
{
    return appContext()->objectDisplaySettings()->objectColor(object);
}

QString ObjectAppearanceProvider::title(const nx::common::metadata::ObjectMetadata& object) const
{
    return QString::fromStdString(
        m_objectTypeDictionary.idToName(object.typeId).value_or(std::string()));
}

const nx::analytics::db::AbstractObjectTypeDictionary* ObjectAppearanceProvider::
    objectTypeDictionary() const
{
    return &m_objectTypeDictionary;
}

} // namespace nx::vms::client::core::analytics
