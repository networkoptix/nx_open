#include "installation_info.h"

#include <QtCore/QFile>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <nx/fusion/model_functions.h>

static const auto kLogTag = nx::utils::log::Tag(QString("nx::vms::utils::InstallationInfo"));

namespace nx::vms::utils {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((InstallationInfo), (json), _Fields)

/**
 * Reads the whole file.
 * @return File contents, or empty string on error, having logged the error message.
 */
static QString readFileIfExists(const QString& filename)
{
    QString result;
    QFile file(filename);

    if (!file.exists())
    {
        NX_DEBUG(kLogTag, "File %1 is not exist", filename);
        return result;
    }

    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        NX_ERROR(kLogTag, "Unable to open file %1", filename);
        return result;
    }

    result = QTextStream(&file).readAll();
    file.close();

    return result;
}

static void loadInstallationInfo(InstallationInfo* installationInfo)
{
    // Currently InstallationInfo is supported only in Linux and macOS.
    if (!nx::utils::AppInfo::isLinux() && !nx::utils::AppInfo::isMacOsX())
        return;

    const QString installationInfoJsonFilename =
        lm("/opt/%1/installation_info.json").arg(nx::utils::AppInfo::linuxOrganizationName());

    const QString content = readFileIfExists(installationInfoJsonFilename);

    if (content.isEmpty())
        return; //< Error is already logged.

    if (!QJson::deserialize(content, installationInfo))
        NX_ERROR(kLogTag, "Invalid JSON in file %1", installationInfoJsonFilename);
}

const InstallationInfo& installationInfo()
{
    static InstallationInfo installationInfo;
    static bool loaded = false;

    if (!loaded)
    {
        loadInstallationInfo(&installationInfo);
        loaded = true;
    }

    return installationInfo;
}

} // namespace nx::vms::utils
