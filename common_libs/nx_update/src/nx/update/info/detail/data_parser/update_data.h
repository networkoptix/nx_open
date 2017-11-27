#pragma once

#include <QtCore>
#include <utils/common/software_version.h>
#include <nx/update/info/file_data.h>

namespace nx {
namespace update {
namespace info {

struct UpdateData
{
    QnSoftwareVersion version;
    QString cloudHost;
    QHash<QString, FileData> targetToPackage;
    QHash<QString, FileData> targetToClientPackage;
};

} // namespace info
} // namespace update
} // namespace nx
