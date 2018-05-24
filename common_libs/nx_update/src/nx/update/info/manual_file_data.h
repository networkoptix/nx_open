#pragma once

#include <QtCore>
#include <utils/common/software_version.h>
#include <nx/utils/uuid.h>
#include <nx/update/info/os_version.h>

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
    QnSoftwareVersion nxVersion;
    QList<QnUuid> peers;
    bool isClient = false;

    ManualFileData(
        const QString& file,
        const OsVersion& osVersion,
        const QnSoftwareVersion& nxVersion,
        bool isClient);

    ManualFileData() = default;
    bool isNull() const;

    static ManualFileData fromFileName(const QString& fileName);
};

NX_UPDATE_API bool operator==(const ManualFileData& lhs, const ManualFileData& rhs);

} // namespace info
} // namespace update
} // namespace nx
