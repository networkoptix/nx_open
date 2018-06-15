#pragma once

#include "id_data.h"

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct LayoutTourItemData: Data
{
    QnUuid resourceId;
    int delayMs = 0;

    LayoutTourItemData() = default;
    LayoutTourItemData(const QnUuid& resourceId, int delayMs):
        resourceId(resourceId), delayMs(delayMs) {}
};
#define LayoutTourItemData_Fields (resourceId)(delayMs)

struct LayoutTourSettings: Data
{
    bool manual = false;
};
#define LayoutTourSettings_Fields (manual)

struct LayoutTourData: IdData
{
    QnUuid parentId;
    QString name;
    LayoutTourItemDataList items;
    LayoutTourSettings settings;

    bool isValid() const { return !id.isNull(); }
};
#define LayoutTourData_Fields IdData_Fields (parentId)(name)(items)(settings)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::LayoutTourData)
