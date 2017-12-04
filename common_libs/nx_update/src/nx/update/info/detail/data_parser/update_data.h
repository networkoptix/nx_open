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
    QHash<QString, FileData> targetToPackage;
    QHash<QString, FileData> targetToClientPackage;
};

} // data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
