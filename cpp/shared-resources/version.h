//
// This file is generated. Go to pom.xml.
//
#ifndef ${artifactId}_VERSION_H
#define ${artifactId}_VERSION_H

/* 
 * Some defines from the build system. Feel free to use these.
 */
#define QN_ORGANIZATION_NAME        "${company.name}"
#define QN_APPLICATION_NAME         "${product.title}"
#define QN_APPLICATION_VERSION      "${release.version}.${buildNumber}"
#define QN_ENGINE_VERSION           "${release.version}.${buildNumber}"
#define QN_APPLICATION_REVISION     "${changeSet}"
#define QN_APPLICATION_PLATFORM     "${platform}"
#define QN_APPLICATION_ARCH         "${arch}"
#define QN_APPLICATION_COMPILER     "${compiler}"
#define QN_FFMPEG_VERSION           "${ffmpeg.version}"
#define QN_SIGAR_VERSION            "${sigar.version}"
#define QN_BOOST_VERSION            "${boost.version}"
#define QN_CUSTOMIZATION_NAME       "${installer.customization}"
#define QN_MEDIA_FOLDER_NAME        "${client.mediafolder.name}"
#define QN_CLIENT_EXECUTABLE_NAME   "${product.name}.exe"
#define QN_LICENSING_MAIL_ADDRESS   "${company.license.address}"
#define QN_SUPPORT_MAIL_ADDRESS     "${company.support.address}"
#define QN_FREE_LICENSE_COUNT       ${freeLicenseCount}
#define QN_FREE_LICENSE_KEY         "${freeLicenseKey}"
#define QN_LICENSE_URL              "${license.url}"
#define QN_RSA_PUBLIC_KEY           ${rsa.public.key}

// TODO: Beauoreeze, define these variables conditionally. 1st for production, 2nd for testing.

#define QN_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n" \
"MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAN4wCk8ISwRsPH0Ev/ljnEygpL9n7PhA\n" \
"EwVi0AB6ht0hQ3sZUtM9UAGrszPJOzFfZlDB2hZ4HFyXfVZcbPxOdmECAwEAAQ==\n" \
"-----END PUBLIC KEY-----";

#define QN_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n" \
"MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBALgdetuLJ0NKq16D5cD6bixLOp27gQDj\n" \
"2Hsq9XbbEx8hOIIvpPOvnvN9LvsJeI8HlbBSnueqll+vxHjQWmgQfmECAwEAAQ==\n" \
"-----END PUBLIC KEY-----"

/* 
 * These constans are here for windows resouce file.
 *
 * DO NOT USE THEM IN YOUR CODE. 
 * DO NOT ADD NEW CONSTANTS HERE.
 */
#define VER_LINUX_ORGANIZATION_NAME "${deb.customization.company.name}" // TODO: move up
#define VER_FILEVERSION             ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion},${buildNumber}
#define VER_FILEVERSION_STR         "${release.version}.${buildNumber}"
#define VER_PRODUCTVERSION          ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion}
#define VER_PRODUCTVERSION_STR      "${release.version}.${buildNumber}"
#define VER_COMPANYNAME_STR         "${company.name}"
#define VER_FILEDESCRIPTION_STR     "${product.title}"
#define VER_INTERNALNAME_STR        "${product.title}"
#define VER_LEGALCOPYRIGHT_STR      "Copyright 2011 Network Optix"
#define VER_LEGALTRADEMARKS1_STR    "All Rights Reserved"
#define VER_LEGALTRADEMARKS2_STR    VER_LEGALTRADEMARKS1_STR
#define VER_ORIGINALFILENAME_STR    "${artifactId}.exe"
#define VER_PRODUCTNAME_STR         "${artifactId}"
#define VER_COMPANYDOMAIN_STR       "${company.url}"
/* BORIS, a note personally to you. If you continue adding defines to this block, I'll rip you a new asshole. You will not enjoy it. */

#endif // ${artifactId}_VERSION_H
