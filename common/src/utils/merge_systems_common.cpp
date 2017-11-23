#include "merge_systems_common.h"

#include <QtCore/QCoreApplication>

#include <nx/network/app_info.h>
#include <network/connection_validator.h>
#include <network/system_helpers.h>

#include <utils/common/app_info.h>

namespace {

using namespace utils::MergeSystemsStatus;

const QHash<Value, QString> kErrorToStringHash{
    { ok, QString() },
    { notFound, lit("FAIL") },
    { incompatibleVersion, lit("INCOMPATIBLE") },
    { unauthorized, lit("UNAUTHORIZED") },
    { forbidden, lit("FORBIDDEN") },
    { notLocalOwner, lit("NOT_LOCAL_OWNER") },
    { backupFailed, lit("BACKUP_ERROR") },
    { starterLicense, lit("STARTER_LICENSE_ERROR") },
    { safeMode, lit("SAFE_MODE") },
    { configurationFailed, lit("CONFIGURATION_ERROR") },
    { dependentSystemBoundToCloud, lit("DEPENDENT_SYSTEM_BOUND_TO_CLOUD") },
    { bothSystemBoundToCloud, lit("BOTH_SYSTEM_BOUND_TO_CLOUD") },
    { differentCloudHost, lit("DIFFERENT_CLOUD_HOST") },
    { unconfiguredSystem, lit("UNCONFIGURED_SYSTEM") },
    { unknownError, lit("UNKNOWN_ERROR") }};

class ErrorStrings
{
    Q_DECLARE_TR_FUNCTIONS(MergeSystemsStatusStrings)

public:
    static QString getErrorMessage(Value value, const QnModuleInformation& moduleInformation)
    {
        const auto systemName = helpers::isNewSystem(moduleInformation)
            ? L'"' + tr("New System") + L'"'
            : moduleInformation.systemName;

        switch (value)
        {
            case ok:
                return QString();
            case notFound:
                return tr("System was not found.");
            case incompatibleVersion:
            {
                //TODO: #GDM Why do we have two separate mechanisms of connection validation?
                const auto status = QnConnectionValidator::validateConnection(moduleInformation);
                if (status == Qn::IncompatibleInternalConnectionResult)
                    return tr("The discovered System %1 is incompatible with the current System.",
                        "%1 is name of System")
                        .arg(systemName);

                return tr("The discovered System %1 has an incompatible version %2.",
                    "%1 is name of System, %2 is version information")
                    .arg(systemName).arg(moduleInformation.version.toString());
            }
            case unauthorized:
                return tr("The password or user name is invalid.");
            case forbidden:
                return tr("This user does not have permissions for the requested operation.");
            case notLocalOwner:
                return tr("Cannot connect to the other System "
                    "because current System is already connected to %1.",
                    "%1 is the cloud name (like 'Nx Cloud')")
                        .arg(nx::network::AppInfo::cloudName());
            case backupFailed:
                return tr("Cannot create database backup.");
            case starterLicense:
                return tr("You are about to merge Systems with Starter licenses.")
                    + L'\n' + tr("Only one Starter license is allowed per System,"
                        " so the second license will be deactivated.")
                    + L'\n' + tr("Merge anyway?");
            case safeMode:
                return tr("The discovered System %1 is in safe mode.",
                    "%1 is name of System")
                    .arg(systemName);
            case configurationFailed:
                return tr("Could not configure the remote System %1.",
                    "%1 is name of System")
                    .arg(systemName);
            case dependentSystemBoundToCloud:
                return tr("Cloud System can only be merged with non-Cloud. "
                    "System name and password are taken from Cloud System."
                    "%1 is the cloud name (like 'Nx Cloud')")
                    .arg(nx::network::AppInfo::cloudName());
            case bothSystemBoundToCloud:
                return tr("Both Systems are connected to %1. Merge is not allowed.",
                    "%1 is the cloud name (like 'Nx Cloud')")
                    .arg(nx::network::AppInfo::cloudName());
            case differentCloudHost:
                return tr("These Systems are built with different %1 URL. Merge is not allowed.",
                    "%1 is the cloud name (like 'Nx Cloud')")
                    .arg(nx::network::AppInfo::cloudName());
            case unconfiguredSystem:
                return tr("System name is not configured yet.");
            default:
                return tr("Unknown error.");
        }
    }
};

} // namespace

namespace utils {
namespace MergeSystemsStatus {

QString toString(Value value)
{
    const auto it = kErrorToStringHash.find(value);
    if (it != kErrorToStringHash.end())
        return it.value();

    return kErrorToStringHash[unknownError];
}

Value fromString(const QString& str)
{
    return kErrorToStringHash.key(str, unknownError);
}

QString getErrorMessage(Value value, const QnModuleInformation& moduleInformation)
{
    return ErrorStrings::getErrorMessage(value, moduleInformation);
}

} // namespace MergeSystemsStatus
} // namespace utils
