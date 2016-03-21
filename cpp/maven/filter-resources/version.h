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

#ifdef _WIN32
#   define QN_CLIENT_EXECUTABLE_NAME    "${product.name}.exe"
#else
#   define QN_CLIENT_EXECUTABLE_NAME    "client-bin"
#endif

 /* These are used in nxtool */
#define QN_APPLICATION_REVISION         "${changeSet}"
#define QN_COMPANY_URL                  "${company.url}"
#define QN_SUPPORT_MAIL_ADDRESS         "${company.support.address}"
#define QN_SUPPORT_LINK                 "${company.support.link}"

/* These are used in old ios client */
#define QN_PRODUCT_NAME_LONG            "${display.product.name}"
#define QN_IOS_PLAYBUTTON_TINT          "${ios.playButton.tint}"
#define QN_IOS_SHARED_GROUP_ID          "${ios.group_identifier}"
#define QN_IOS_NEW_APP_APPSTORE_ID      "${ios.new_app_appstore_id}"

#define VER_LINUX_ORGANIZATION_NAME     "${deb.customization.company.name}"

/*
 * Really, guys!
 *
 * DO NOT USE THESE CONSTANTS IN YOUR CODE.
 * DO NOT ADD NEW CONSTANTS HERE.
 *
 */
