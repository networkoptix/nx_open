#pragma once

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <nx/utils/uuid.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace client {
namespace desktop {

class ObjectDisplaySettings: public QObject
{
    Q_OBJECT

public:
    ObjectDisplaySettings();
    virtual ~ObjectDisplaySettings() override;

    QColor objectColor(const QnUuid& objectTypeId);
    QColor objectColor(const common::metadata::DetectedObject& object);
    std::vector<common::metadata::Attribute> briefAttributes(
        const common::metadata::DetectedObject& object) const;
    std::vector<common::metadata::Attribute> visibleAttributes(
        const common::metadata::DetectedObject& object) const;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
