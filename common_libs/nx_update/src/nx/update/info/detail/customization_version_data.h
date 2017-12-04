#pragma once

#include <utils/common/software_version.h>
#include <nx/update/info/detail/data_parser/update_data.h>

namespace nx {
namespace update {
namespace info {
namespace detail {

struct CustomizationVersionData
{
    QString name;
    QnSoftwareVersion version;

    CustomizationVersionData(const QString& name, const QnSoftwareVersion& version):
        name(name),
        version(version)
    {}

    CustomizationVersionData() = default;
};

inline bool operator < (const CustomizationVersionData& lhs, const CustomizationVersionData& rhs)
{
    return lhs.name < rhs.name
        ? true
        : rhs.name < lhs.name
            ? false
            : lhs.version < rhs.version;
}

using CustomizationVersionToUpdate = QMap<CustomizationVersionData, data_parser::UpdateData>;

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
