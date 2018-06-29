#pragma once

#include <QtCore>

#include <nx/utils/uuid.h>
#include <nx/update/info/os_version.h>
#include <nx/utils/software_version.h>

namespace nx {
namespace update {
namespace info {

/**
 * Update file information for manual addition.
 */
struct NX_UPDATE_API ManualFileData
{
    QString file;
    OsVersion osVersion;
    nx::utils::SoftwareVersion nxVersion;
    QList<QnUuid> peers;
    bool isClient = false;

    ManualFileData(
        const QString& file,
        const OsVersion& osVersion,
        const nx::utils::SoftwareVersion& nxVersion,
        bool isClient);

    ManualFileData() = default;
    bool isNull() const;

    static ManualFileData fromFileName(const QString& fileName);
};

NX_UPDATE_API bool operator==(const ManualFileData& lhs, const ManualFileData& rhs);

} // namespace info
} // namespace update
} // namespace nx
