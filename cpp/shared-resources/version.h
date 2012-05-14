#ifndef ${artifactId}_VERSION_H_
#define ${artifactId}_VERSION_H_
// This file is generated. Go to version.py or pom.xml
static const char* ORGANIZATION_NAME="${pom.parent.organization.name}";
static const char* APPLICATION_NAME="${project.name}";
static const char* APPLICATION_VERSION="${project.version}.${buildNumber}";
const char* const APPLICATION_REVISION="${changeSet}";
const char* const FFMPEG_VERSION="${ffmpeg.version}";

// These constans are here for windows resouce file.
#define VER_FILEVERSION             ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion},${buildNumber}
#define VER_FILEVERSION_STR         "${project.version}.${buildNumber}"
#define VER_PRODUCTVERSION          ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion}
#define VER_PRODUCTVERSION_STR      "${project.version}.${buildNumber}"
#define VER_COMPANYNAME_STR         "${pom.parent.organization.name}"
#define VER_FILEDESCRIPTION_STR     "${project.name}"
#define VER_INTERNALNAME_STR        "${project.name}"
#define VER_LEGALCOPYRIGHT_STR      "Copyright © 2011 Network Optix"
#define VER_LEGALTRADEMARKS1_STR    "All Rights Reserved"
#define VER_LEGALTRADEMARKS2_STR    VER_LEGALTRADEMARKS1_STR
#define VER_ORIGINALFILENAME_STR    "${artifactId}.exe"
#define VER_PRODUCTNAME_STR         "${artifactId}"
#define VER_COMPANYDOMAIN_STR       "${pom.parent.organization.url}"
#endif // ${artifactId}_VERSION_H_
