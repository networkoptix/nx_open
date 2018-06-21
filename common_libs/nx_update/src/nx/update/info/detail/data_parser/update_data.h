#pragma once

#include <QtCore>
#include <utils/common/software_version.h>
#include <nx/update/info/file_data.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

struct UpdateData
{
    QString cloudHost;
    QString eulaLink;
    int eulaVersion = -1;
    QHash<QString, FileData> targetToPackage;
    QHash<QString, FileData> targetToClientPackage;
};

inline bool operator == (const UpdateData& lhs, const UpdateData& rhs)
{
    return lhs.cloudHost == rhs.cloudHost
        && lhs.targetToPackage == rhs.targetToPackage
        && lhs.targetToClientPackage == rhs.targetToClientPackage
        && lhs.eulaLink == rhs.eulaLink
        && lhs.eulaVersion == rhs.eulaVersion;
}

} // data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
