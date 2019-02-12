#include "update_information.h"
#include <QJsonObject>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/utils/scope_guard.h>

namespace nx::update {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::update, InformationError,
    (nx::update::InformationError::noError, "no error")
    (nx::update::InformationError::networkError, "network error")
    (nx::update::InformationError::httpError, "http error")
    (nx::update::InformationError::jsonError, "json error")
    (nx::update::InformationError::brokenPackageError, "local update package is corrupted")
    (nx::update::InformationError::missingPackageError, "missing files in the update package")
    (nx::update::InformationError::incompatibleVersion, "incompatible version")
    (nx::update::InformationError::notFoundError, "not found")
    (nx::update::InformationError::noNewVersion, "no new version"))

QString toString(InformationError error)
{
    QString result;
    QnLexical::serialize(error, &result);
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::update::Package, (xml)(csv_record)(ubjson)(json)(datastream)(eq), Package_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::update::Information, (xml)(csv_record)(ubjson)(json)(datastream)(eq), Information_Fields)

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::update::Status, Code)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::update::Status, (xml)(csv_record)(ubjson)(json), UpdateStatus_Fields)

nx::utils::SoftwareVersion UpdateContents::getVersion() const
{
    return nx::utils::SoftwareVersion(info.version);
}

bool UpdateContents::isValidToInstall() const
{
    // Ignoring all errors when update is already running.
    if (sourceType == UpdateSourceType::mediaservers)
        return true;
    return missingUpdate.empty()
        && unsuportedSystemsReport.empty()
        && !info.version.isEmpty()
        && invalidVersion.empty()
        && error == nx::update::InformationError::noError;
}

bool UpdateContents::isEmpty() const
{
    return info.packages.empty();
}

bool UpdateContents::preferOtherUpdate(const UpdateContents& other) const
{
    if (isEmpty())
        return true;

    // Prefere update from mediaservers.
    if (sourceType != UpdateSourceType::mediaservers
        && other.sourceType == UpdateSourceType::mediaservers)
    {
        return true;
    }
    else if (sourceType != UpdateSourceType::mediaservers
        && other.sourceType == UpdateSourceType::mediaservers)
    {
        return false;
    }

    return other.getVersion() > getVersion();
}

} // namespace nx::update
