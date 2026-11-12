// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtGui/QColor>

#include <analytics/common/object_metadata.h>
#include <nx/analytics/taxonomy/object_type_dictionary.h>

namespace nx::vms::client::core {

class SystemContext;

namespace analytics {

/** How an analytics object is presented to the user. */
class NX_VMS_CLIENT_CORE_API AbstractObjectAppearanceProvider
{
public:
    virtual ~AbstractObjectAppearanceProvider() = default;

    virtual QColor color(const nx::common::metadata::ObjectMetadata& object) const = 0;
    virtual QString title(const nx::common::metadata::ObjectMetadata& object) const = 0;
};

/**
 * Takes colors from the application ObjectDisplaySettings and object type names from the taxonomy
 * of the given System.
 */
class NX_VMS_CLIENT_CORE_API ObjectAppearanceProvider: public AbstractObjectAppearanceProvider
{
public:
    explicit ObjectAppearanceProvider(SystemContext* systemContext);

    virtual QColor color(const nx::common::metadata::ObjectMetadata& object) const override;
    virtual QString title(const nx::common::metadata::ObjectMetadata& object) const override;

    /** Object type dictionary of the System, which the titles are taken from. */
    const nx::analytics::db::AbstractObjectTypeDictionary* objectTypeDictionary() const;

private:
    const nx::analytics::taxonomy::ObjectTypeDictionary m_objectTypeDictionary;
};

} // namespace analytics
} // namespace nx::vms::client::core
