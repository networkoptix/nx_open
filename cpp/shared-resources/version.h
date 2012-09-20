//
// This file is generated. Go to pom.xml.
//
#ifndef ${artifactId}_VERSION_H_
#define ${artifactId}_VERSION_H_

static const char *const ORGANIZATION_NAME="${company.name}";
static const char *const APPLICATION_NAME="${product.title}";
static const char *const APPLICATION_VERSION="${project.version}.${buildNumber}";
static const char *const APPLICATION_REVISION="${changeSet}";
static const char *const FFMPEG_VERSION="${ffmpeg.version}";

// These constans are here for windows resouce file.
#define VER_CUSTOMIZATION           "${installer.customization}"
#define VER_LINUX_ORGANIZATION_NAME "${deb.customization.company.name}"
#define VER_FILEVERSION             ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion},${buildNumber}
#define VER_FILEVERSION_STR         "${project.version}.${buildNumber}"
#define VER_PRODUCTVERSION          ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion}
#define VER_PRODUCTVERSION_STR      "${project.version}.${buildNumber}"
#define VER_COMPANYNAME_STR         "${company.name}"
#define VER_FILEDESCRIPTION_STR     "${product.title}"
#define VER_INTERNALNAME_STR        "${product.title}"
#define VER_LEGALCOPYRIGHT_STR      "Copyright © 2011 Network Optix"
#define VER_LEGALTRADEMARKS1_STR    "All Rights Reserved"
#define VER_LEGALTRADEMARKS2_STR    VER_LEGALTRADEMARKS1_STR
#define VER_ORIGINALFILENAME_STR    "${artifactId}.exe"
#define VER_PRODUCTNAME_STR         "${artifactId}"
#define VER_COMPANYDOMAIN_STR       "${company.url}"

#endif // ${artifactId}_VERSION_H_
