#pragma once

#include "nx/utils/software_version.h"

namespace nx {
namespace client {
namespace desktop {

// This data is extracted from updates.json
struct CustomizationInfo
{
    QString current_release;
    QString updates_prefix;
    QString release_notes;
    QString description;
    QMap<QString, nx::utils::SoftwareVersion> releases;
};

// Contains full info about update package.
// This data is extracted from every record of update.json.
struct UpdatePackageInfo
{
    QString md5;
    QString file;       //< Local filename. baseFileName
    qint64 size = 0;    // former fileSize
    nx::utils::Url url; //< URL to this file
    QString platform;   //< One of {"linux", "windows", "macosx" }
    QString variant;    //< One of {"x64_ubuntu", "x64_winxp" }
    nx::utils::SoftwareVersion version;

    // Reset all data to initial state.
    void clear();
    // Check if there is some file data inside
    bool isValid() const;
};

// All information from customization/build specific update.json is stored here
struct BuildInformation
{
    //using PackagesHash = QHash<QString, QHash<QString, UpdateFileInformation>>;
    using PackagesHash = std::vector<UpdatePackageInfo>;
    PackagesHash packages;
    PackagesHash clientPackages;
    nx::utils::SoftwareVersion version;
    QString cloudHost;
    nx::utils::SoftwareVersion minimalClientVersion;
    int eulaVersion = 0;
    QString eulaLink;
};


} // namespace desktop
} // namespace client
} // namespace nx
