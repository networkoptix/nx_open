#pragma once

#include <QtCore/QString>
#include <network/module_information.h>

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
};

QString toString(Value value);
Value fromString(const QString& str);
QString getErrorMessage(Value value, const QnModuleInformation& moduleInformation);

} // namespace MergeSystemsStatus

} // namespace utils
