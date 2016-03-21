//
// This file is generated. Go to pom.xml.
//
#pragma once

/* 
 * Some defines from the build system. Using them directly strongly not recommended.
 * 
 * DO NOT USE THEM IN YOUR CODE. 
 * DO NOT ADD NEW CONSTANTS HERE.
 *
 * All projects that are depend on "common" should use <utils/common/app_info.h> instead.
 * The QN_APPLICATION_NAME should be read from QCoreApplication::applicationName().
 *
 * No new variables should be added here. The major part of the file will be wiped out asap.
 */
#define QN_BETA                         "${beta}"
#define QN_ORGANIZATION_NAME            "${company.name}"
#define QN_APPLICATION_NAME             "${product.title}"
#define QN_APPLICATION_DISPLAY_NAME     "${product.display.title}"
#define QN_APPLICATION_VERSION          "${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}"
#ifdef _WIN32
#   define QN_PRODUCT_NAME              "${product.name}"
#else
#   define QN_PRODUCT_NAME              "${namespace.additional}"
#endif
#define QN_PRODUCT_NAME_SHORT           "${product.name.short}"
#define QN_ENGINE_VERSION               "${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}"
#define QN_APPLICATION_REVISION         "${changeSet}"
#define QN_APPLICATION_PLATFORM         "${platform}"
#define QN_APPLICATION_ARCH             "${arch}"
#define QN_APPLICATION_COMPILER         "${additional.compiler}"
#define QN_APPLICATION_MODIFICATION     "${modification}"
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
#define QN_SUPPORT_LINK                 "${company.support.link}"
#define QN_FREE_LICENSE_COUNT           ${freeLicenseCount}
#define QN_FREE_LICENSE_KEY             "${freeLicenseKey}"
#define QN_FREE_LICENSE_IS_TRIAL        ${freeLicenseIsTrial}
#define QN_SHOWCASE_URL                 "${showcase.url}/${customization}"
#define QN_SETTINGS_URL                 "${settings.url}/${customization}.json"
#define QN_PRODUCT_NAME_LONG            "${display.product.name}"
#define QN_BUILDENV_PATH                "${environment.dir}"
#define QN_MIRRORLIST_URL               "${mirrorListUrl}"
#define QN_HELP_URL                     "${helpUrl}/${customization}/${parsedVersion.majorVersion}/${parsedVersion.minorVersion}/url"
#define QN_ARM_BOX                      "${box}"
#define QN_IOS_PLAYBUTTON_TINT          "${ios.playButton.tint}"
#define QN_IOS_SHARED_GROUP_ID          "${ios.group_identifier}"
#define QN_IOS_NEW_APP_APPSTORE_ID      "${ios.new_app_appstore_id}"

#define QN_AX_CLASS_ID                  "${ax.classId}"
#define QN_AX_INTERFACE_ID              "${ax.interfaceId}"
#define QN_AX_EVENTS_ID                 "${ax.eventsId}"
#define QN_AX_TYPELIB_ID                "${ax.typeLibId}"
#define QN_AX_APP_ID                    "${ax.appId}"

#define VER_LINUX_ORGANIZATION_NAME     "${deb.customization.company.name}"

/* 
 * Really, guys!
 * 
 * DO NOT USE THESE CONSTANTS IN YOUR CODE. 
 * DO NOT ADD NEW CONSTANTS HERE.
 *
 */
