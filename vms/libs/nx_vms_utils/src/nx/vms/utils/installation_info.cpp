// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "installation_info.h"

#include <QtCore/QFile>

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

#include <nx/branding.h>
#include <nx/build_info.h>

static const auto kLogTag = nx::log::Tag(QString("nx::vms::utils::InstallationInfo"));

namespace nx::vms::utils {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(InstallationInfo, (json), InstallationInfo_Fields)

/**
 * Reads the whole file.
 * @return File contents, or empty string on error, having logged the error message.
 */
static QString readFileIfExists(const QString& filename)
{
    QFile file(filename);

    if (!file.exists())
    {
        NX_DEBUG(kLogTag, "File %1 does not exist", filename);
        return QString();
    }

    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        NX_ERROR(kLogTag, "Unable to open file %1", filename);
        return QString();
    }

    return QTextStream(&file).readAll();
}

static void loadInstallationInfo(InstallationInfo* installationInfo)
{
    // Currently InstallationInfo is supported only in Linux and macOS.
    if (!nx::build_info::isLinux() && !nx::build_info::isMacOsX())
        return;

    const QString installationInfoJsonFilename =
        nx::format("/opt/%1/installation_info.json").arg(nx::branding::companyId());

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
