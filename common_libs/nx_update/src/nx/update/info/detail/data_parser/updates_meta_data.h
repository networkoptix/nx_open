#pragma once

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

    AlternativeServerData(const QString& url, const QString& name):
        url(url),
        name(name)
    {}

    AlternativeServerData() = default;
};

inline bool operator == (const AlternativeServerData& lhs, const AlternativeServerData& rhs)
{
    return lhs.url == rhs.url && lhs.name == rhs.name;
}

struct CustomizationData
{
    QString name;
    QList<QnSoftwareVersion> versions;
};

inline bool operator == (const CustomizationData& lhs, const CustomizationData& rhs)
{
    return lhs.name == rhs.name && lhs.versions == rhs.versions;
}

struct UpdatesMetaData
{
    // todo: add data structure for storing information about unsupported versions
    QList<CustomizationData> customizationDataList;
    QList<AlternativeServerData> alternativeServersDataList;
};

inline bool operator == (const UpdatesMetaData& lhs, const UpdatesMetaData& rhs)
{
    return lhs.customizationDataList == rhs.customizationDataList
        && lhs.alternativeServersDataList == rhs.alternativeServersDataList;
}

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
