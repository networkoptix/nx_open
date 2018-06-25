#pragma once

#include <nx/utils/software_version.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

struct AlternativeServerData
{
    QString url;
    QString name;

    AlternativeServerData() = default;
    AlternativeServerData(const QString& url, const QString& name): url(url), name(name) {}
};

inline bool operator == (const AlternativeServerData& lhs, const AlternativeServerData& rhs)
{
    return lhs.url == rhs.url && lhs.name == rhs.name;
}

struct CustomizationData
{
    QString name;
    QString updatePrefix;
    QList<nx::utils::SoftwareVersion> versions;
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
    int updateManifestVersion = -1;
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
