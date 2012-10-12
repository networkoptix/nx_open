//
// This file is generated. Go to pom.xml.
//
#ifndef ${artifactId}_VERSION_H
#define ${artifactId}_VERSION_H

/* Some defines from the build system. Feel free to use these. */
#define ORGANIZATION_NAME           "${company.name}";
#define APPLICATION_NAME            "${product.title}";
#define APPLICATION_VERSION         "${release.version}.${buildNumber}";
#define APPLICATION_REVISION        "${changeSet}";
#define APPLICATION_PLATFORM        "${platform}";
#define APPLICATION_ARCH            "${arch}";
#define APPLICATION_COMPILER        "${compiler}";
#define FFMPEG_VERSION              "${ffmpeg.version}";
#define SIGAR_VERSION               "${sigar.version}";
#define BOOST_VERSION               "${boost.version}";

/* These constans are here for windows resouce file.
 *
 * DO NOT USE THEM IN YOUR CODE. */
#define VER_CUSTOMIZATION           "${installer.customization}"
#define VER_LINUX_ORGANIZATION_NAME "${deb.customization.company.name}"
#define VER_FILEVERSION             ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion},${buildNumber}
#define VER_FILEVERSION_STR         "${project.version}.${buildNumber}"
#define VER_PRODUCTVERSION          ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion}
#define VER_PRODUCTVERSION_STR      "${project.version}.${buildNumber}"
#define VER_COMPANYNAME_STR         "${company.name}"
#define VER_FILEDESCRIPTION_STR     "${product.title}"
#define VER_INTERNALNAME_STR        "${product.title}"
#define VER_LEGALCOPYRIGHT_STR      "Copyright 2011 Network Optix"
#define VER_LEGALTRADEMARKS1_STR    "All Rights Reserved"
#define VER_LEGALTRADEMARKS2_STR    VER_LEGALTRADEMARKS1_STR
#define VER_ORIGINALFILENAME_STR    "${artifactId}.exe"
#define VER_PRODUCTNAME_STR         "${artifactId}"
#define VER_COMPANYDOMAIN_STR       "${company.url}"

#endif // ${artifactId}_VERSION_H
