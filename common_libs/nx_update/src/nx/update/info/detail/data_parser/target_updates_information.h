#pragma once

#include <QtCore>
#include <nx/update/info/file_information.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

struct TargetUpdatesInformation
{
    QString targetName;
    QList<FileInformation> fileInformationList;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
