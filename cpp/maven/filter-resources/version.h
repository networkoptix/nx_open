//
// This file is generated. Go to pom.xml.
//
#ifndef ${project.artifactId}_VERSION_H
#define ${project.artifactId}_VERSION_H

/* 
 * Some defines from the build system. Feel free to use these.
 */
#define QN_BETA                         "${beta}"
#define QN_ORGANIZATION_NAME            "${company.name}"
#define QN_APPLICATION_NAME             "${product.title}"
#define QN_APPLICATION_VERSION          "${release.version}.${buildNumber}"
#ifdef _WIN32
#   define QN_PRODUCT_NAME              "${product.name}"
#else
#   define QN_PRODUCT_NAME              "${namespace.additional}"
#endif
#define QN_PRODUCT_NAME_SHORT           "${product.name.short}"
#define QN_ENGINE_VERSION               "${release.version}.${buildNumber}"
#define QN_APPLICATION_REVISION         "${changeSet}"
#define QN_APPLICATION_PLATFORM         "${platform}"
#define QN_APPLICATION_ARCH             "${arch}"
#define QN_APPLICATION_COMPILER         "${additional.compiler}"
#define QN_FFMPEG_VERSION               "${ffmpeg.version}"
#define QN_SIGAR_VERSION                "${sigar.version}"
#define QN_BOOST_VERSION                "${boost.version}"
#define QN_CUSTOMIZATION_NAME           "${customization}"
#define QN_MEDIA_FOLDER_NAME            "${client.mediafolder.name}"
#ifdef _WIN32
#   define QN_CLIENT_EXECUTABLE_NAME    "${product.name}.exe"
#   define QN_APPLAUNCHER_EXECUTABLE_NAME "${product.name} Launcher.exe" // TODO: #Boris probably there exists a variable for this?
#else
#   define QN_CLIENT_EXECUTABLE_NAME    "client-bin"
#   define QN_APPLAUNCHER_EXECUTABLE_NAME "applauncher-bin"
#endif
#define QN_LICENSING_MAIL_ADDRESS       "${company.license.address}"
#define QN_COMPANY_URL                  "${company.url}"
#define QN_SUPPORT_MAIL_ADDRESS         "${company.support.address}"
#define QN_FREE_LICENSE_COUNT           ${freeLicenseCount}
#define QN_FREE_LICENSE_KEY             "${freeLicenseKey}"
#define QN_FREE_LICENSE_IS_TRIAL        ${freeLicenseIsTrial}
#define QN_SHOWCASE_URL                 "${showcase.url}"
#define QN_SETTINGS_URL                 "${settings.url}"
#define QN_PRODUCT_NAME_LONG            "${product.name}"
#define QN_BUILDENV_PATH                "${environment.dir}"
#define QN_MIRRORLIST_URL               "${mirrorListUrl}"
/* 
 * These constants are here for windows resource file.
 *
 * DO NOT USE THEM IN YOUR CODE. 
 * DO NOT ADD NEW CONSTANTS HERE.
 */
#define VER_LINUX_ORGANIZATION_NAME     "${deb.customization.company.name}" // TODO: #Elric move up
#define VER_FILEVERSION                 ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion},${buildNumber}
#define VER_FILEVERSION_STR             "${release.version}.${buildNumber}"
#define VER_PRODUCTVERSION              ${parsedVersion.majorVersion},${parsedVersion.minorVersion},${parsedVersion.incrementalVersion}
#define VER_PRODUCTVERSION_STR          "${release.version}.${buildNumber}"
#define VER_COMPANYNAME_STR             "${company.name}"
#define VER_FILEDESCRIPTION_STR         "${product.title}"
#define VER_INTERNALNAME_STR            "${product.title}"
#define VER_LEGALCOPYRIGHT_STR          "Copyright (c) 2011-2013 Network Optix"
#define VER_LEGALTRADEMARKS1_STR        "All Rights Reserved"
#define VER_LEGALTRADEMARKS2_STR        VER_LEGALTRADEMARKS1_STR
#define VER_ORIGINALFILENAME_STR        "${project.artifactId}.exe"
#define VER_PRODUCTNAME_STR             "${project.artifactId}"
#define VER_COMPANYDOMAIN_STR           "${company.url}"
/* BORIS, a note personally to you. If you continue adding defines to this block, I'll rip you a new asshole. You will not enjoy it. */

#endif // ${project.artifactId}_VERSION_H
