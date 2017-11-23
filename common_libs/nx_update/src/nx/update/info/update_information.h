#pragma once

#include <QtCore>
#include "file_information.h"

namespace nx {
namespace update {
namespace info {

struct UpdateInformation
{
    QString version;
    QString cloudHost;
    nx::utils::Url baseUrl;
    QHash<QString, FileInformation> targetToPackage;
    QHash<QString, FileInformation> targetToClientPackage;
};

} // namespace info
} // namespace update
} // namespace nx
