// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

#include <analytics/common/object_metadata.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/settings/types/detected_object.h>

namespace nx::vms::client::desktop {

class ObjectDisplaySettings
{
public:
    ObjectDisplaySettings();

    QColor objectColor(const QString& objectTypeId);
    QColor objectColor(const nx::common::metadata::ObjectMetadata& object);

    std::vector<nx::common::metadata::Attribute> visibleAttributes(
        const nx::common::metadata::ObjectMetadata& object) const;

protected:
    /**
     * Palette used for nx.sys.color attribute value; defined in [attributes.md](attributes.md).
     */
    static const std::map</*name*/ QString, /*hexRgb*/ QString> kBoundingBoxPalette;

private:
    DetectedObjectSettingsMap m_settingsMap;
};

} // namespace nx::vms::client::desktop
