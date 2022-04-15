// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <nx/utils/uuid.h>
#include <analytics/common/object_metadata.h>

namespace nx::vms::client::desktop {

class ObjectDisplaySettings: public QObject
{
    Q_OBJECT

public:
    ObjectDisplaySettings(QObject* parent = nullptr);
    virtual ~ObjectDisplaySettings() override;

    QColor objectColor(const QString& objectTypeId);
    QColor objectColor(const nx::common::metadata::ObjectMetadata& object);

    std::vector<nx::common::metadata::Attribute> visibleAttributes(
        const nx::common::metadata::ObjectMetadata& object) const;

protected:
    /**
     * Palette used for nx.sys.color attribute value; defined in [attributes.md](attributes.md).
     */
    static const std::map</*name*/ std::string, /*hexRgb*/ std::string> kBoundingBoxPalette;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
