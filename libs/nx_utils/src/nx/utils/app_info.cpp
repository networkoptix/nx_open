// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "app_info.h"

#include <nx/branding.h>
#include <nx/build_info.h>

namespace nx::utils {

using namespace nx::build_info;
using namespace nx::branding;

QString AppInfo::applicationFullVersion()
{
    static const QString kFullVersion = QString("%1-%2-%3%4")
        .arg(vmsVersion())
        .arg(revision())
        .arg(customization().replace(' ', '_'))
        .arg(publicationTypeSuffix());

    return kFullVersion;
}

QString AppInfo::organizationNameForSettings()
{
    if (isWindows())
        return company();
    return companyId();
}

} // namespace nx::utils
