#pragma once

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <nx/utils/uuid.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx::vms::client::desktop {

class ObjectDisplaySettings: public QObject
{
    Q_OBJECT

public:
    ObjectDisplaySettings();
    virtual ~ObjectDisplaySettings() override;

    QColor objectColor(const QString& objectTypeId);
    QColor objectColor(const nx::common::metadata::DetectedObject& object);
    std::vector<nx::common::metadata::Attribute> briefAttributes(
        const nx::common::metadata::DetectedObject& object) const;
    std::vector<nx::common::metadata::Attribute> visibleAttributes(
        const nx::common::metadata::DetectedObject& object) const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
