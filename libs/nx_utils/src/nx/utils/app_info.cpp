// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "app_info.h"

#include <nx/branding.h>
#include <nx/build_info.h>

namespace nx::utils {

using namespace nx::build_info;
using namespace nx::branding;

QString AppInfo::appFullVersion(const QString& version)
{
    return QString("%1-%2-%3%4")
        .arg(version)
        .arg(revision())
        .arg(customization().replace(' ', '_'))
        .arg(publicationTypeSuffix());
}

QString AppInfo::vmsFullVersion()
{
    static const QString kFullVersion = appFullVersion(vmsVersion());
    return kFullVersion;
}

QString AppInfo::organizationNameForSettings()
{
    if (isWindows())
        return company();
    return companyId();
}

} // namespace nx::utils
