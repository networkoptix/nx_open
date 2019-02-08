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
    unconfiguredSystem,
    notLocalOwner,
    differentCloudHost,
    duplicateMediaServerFound,
};

QString toString(Value value);
Value fromString(const QString& str);
QString getErrorMessage(Value value, const nx::vms::api::ModuleInformation& moduleInformation);

} // namespace MergeSystemsStatus

} // namespace utils
