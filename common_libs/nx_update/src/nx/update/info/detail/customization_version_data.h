#pragma once

#include <nx/update/info/detail/data_parser/update_data.h>
#include <nx/utils/software_version.h>

namespace nx {
namespace update {
namespace info {
namespace detail {

struct CustomizationVersionData
{
    QString name;
    nx::utils::SoftwareVersion version;

    CustomizationVersionData() = default;
    CustomizationVersionData(const QString& name, const nx::utils::SoftwareVersion& version):
        name(name),
        version(version)
    {
    }
};

inline bool operator<(const CustomizationVersionData& lhs, const CustomizationVersionData& rhs)
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
