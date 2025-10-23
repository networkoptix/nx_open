// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::utils {

class NX_UTILS_API AppInfo
{
public:
    /**
     * Returns full version including commit hash, customization and publication type for
     * specified product version.
     */
    static QString appFullVersion(const QString& version);

    /** Returns full VMS version including commit hash, customization and publication type. */
    static QString vmsFullVersion();

    static QString organizationNameForSettings();
};

} // namespace nx::utils
