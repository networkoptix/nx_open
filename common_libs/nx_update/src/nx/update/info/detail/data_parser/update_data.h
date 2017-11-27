#pragma once

#include <QtCore>
#include <nx/update/info/file_data.h>

namespace nx {
namespace update {
namespace info {

struct UpdateData
{
    QString version;
    QString cloudHost;
    QHash<QString, FileData> targetToPackage;
    QHash<QString, FileData> targetToClientPackage;
};

} // namespace info
} // namespace update
} // namespace nx
