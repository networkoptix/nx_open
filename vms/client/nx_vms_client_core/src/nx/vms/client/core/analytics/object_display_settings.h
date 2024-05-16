// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <QtGui/QColor>

#include <analytics/common/object_metadata.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ObjectDisplaySettings
{
public:
    QColor objectColor(const QString& objectTypeId);
    QColor objectColor(const nx::common::metadata::ObjectMetadata& object);

    std::vector<nx::common::metadata::Attribute> visibleAttributes(
        const nx::common::metadata::ObjectMetadata& object) const;

protected:
    /**
     * Palette used for nx.sys.color attribute value; defined in [attributes.md](attributes.md).
     */
    static const std::map</*name*/ std::string, /*hexRgb*/ std::string> kBoundingBoxPalette;
};

} // namespace nx::vms::client::core
