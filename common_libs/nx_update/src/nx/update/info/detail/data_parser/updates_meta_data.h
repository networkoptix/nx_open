#pragma once

#include <QtCore>
#include <utils/common/software_version.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

struct AlternativeServerData
{
    QString url;
    QString name;
};

struct CustomizationData
{
    QString name;
    QList<QnSoftwareVersion> versions;
};

struct UpdatesMetaData
{
    // todo: add data structure for storing information about unsupported versions 
    QList<CustomizationData> customizationDataList;
    QList<AlternativeServerData> alternativeServersDataList;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
