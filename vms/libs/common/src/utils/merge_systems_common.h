#pragma once

#include <QtCore/QString>
#include <nx/vms/api/data_fwd.h>

namespace utils {
namespace MergeSystemsStatus {

enum Value
{
    ok,
    unknownError,
    notFound,
    incompatibleVersion,
    unauthorized,
    forbidden,
    configurationFailed,
    backupFailed,
    starterLicense,
    safeMode,
    dependentSystemBoundToCloud,
    bothSystemBoundToCloud,
    cloudSystemsHaveDifferentOwners,
    unconfiguredSystem,
    differentCloudHost,
    duplicateMediaServerFound,
    nvrLicense,
};

QString toString(Value value);
Value fromString(const QString& str);
QString getErrorMessage(Value value, const nx::vms::api::ModuleInformation& moduleInformation);

/** Check if systems are allowed to merge. */
bool allowsToMerge(Value value);

} // namespace MergeSystemsStatus
} // namespace utils
