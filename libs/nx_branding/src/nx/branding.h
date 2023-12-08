// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace nx::branding {

/** Customization id. */
NX_BRANDING_API QString customization();

/**
 * Customization brand. Used for the license compatibility checks.
 */
NX_BRANDING_API QString brand();

/** Company name. */
NX_BRANDING_API QString company();

/** Internal company id. Used as a file paths part on linux. */
NX_BRANDING_API QString companyId();

/** VMS name. Used for the display purposes only. */
NX_BRANDING_API QString vmsName();

/** Full cloud name, including brand. */
NX_BRANDING_API QString cloudName();

/** Whether cloud host is for development purposes - like stage/test/demo etc.  */
NX_BRANDING_API bool isDevCloudHost();

/** Short cloud name, usually does not include brand (e.g. 'cloud'), but rarely does. */
NX_BRANDING_API QString shortCloudName();

/** Cloud host. */
NX_BRANDING_API QString cloudHost();

/** List of compatible customizations for the mobile application. */
NX_BRANDING_API QStringList compatibleCustomizations();

/** List of compatible cloud hosts for the mobile application. */
NX_BRANDING_API QStringList compatibleCloudHosts();

/** List of compatible uri protocols for the mobile application. */
NX_BRANDING_API QStringList compatibleUriProtocols();

/** Default locale code for all ui applications. */
NX_BRANDING_API QString defaultLocale();

/** Native protocol for URI links. */
NX_BRANDING_API QString nativeUriProtocol();

/** Version of the EULA. */
NX_BRANDING_API int eulaVersion();

/** Support address. Can be email, web or phone number. */
NX_BRANDING_API QString supportAddress();

/** Licensing address. Can be email or web address. */
NX_BRANDING_API QString licensingAddress();

/** Directory where all software is installed by default. */
NX_BRANDING_API QString installationRoot();

/** Folder name for the server archive and client media files.  */
NX_BRANDING_API QString mediaFolderName();

/** Main company web page address. */
NX_BRANDING_API QString companyUrl();

/** File with current version of the applauncher. */
NX_BRANDING_API QString launcherVersionFile();

/** Trial license key for the selected customization. */
NX_BRANDING_API QString freeLicenseKey();

/**
 * Name of the default (up to 2.5) installed applauncher binary in the filesystem (applauncher,
 * HD Witness Launcher.exe).
 */
NX_BRANDING_API QString minilauncherBinaryName();

/** Name of the new applauncher binary in the filesystem (applauncher-bin, applauncher.exe). */
NX_BRANDING_API QString applauncherBinaryName();

/** Internal name, used as a key in settings storage, data folder, etc. */
NX_BRANDING_API QString applauncherInternalName();

/** Name of the client binary in the filesystem (client-bin, HD Witness.exe). */
NX_BRANDING_API QString desktopClientBinaryName();

/**
 * Name of the client launcher in the filesystem. On Windows and MacOS actual client binary is
 * used (HD Witness.exe). On Linux separate `client` launcher is used.
 */
NX_BRANDING_API QString desktopClientLauncherName();

/** Internal name, used as a key in settings storage, data folder, etc. */
NX_BRANDING_API QString desktopClientInternalName();

/** Real application name, visible to the user. */
NX_BRANDING_API QString desktopClientDisplayName();

/** Shortcut to Mobile Help name. */
NX_BRANDING_API QString mobileHelpShortcutName();

/** Mobile Help document name. */
NX_BRANDING_API QString mobileHelpDocumentName();

/** Mobile Help file name. */
NX_BRANDING_API QString mobileHelpFileName();

/** Quick Start Guide document name. */
NX_BRANDING_API QString quickStartGuideDocumentName();

/** Shortcut to Quick Start Guide name. */
NX_BRANDING_API QString quickStartGuideShortcutName();

/** Quick Start Guide file name. */
NX_BRANDING_API QString quickStartGuideFileName();

/** Internal name, used as a key in settings storage, data folder, etc. */
NX_BRANDING_API QString mobileClientInternalName();

/** Real application name, visible to the user. */
NX_BRANDING_API QString mobileClientDisplayName();

/** Id of the old android mobile application. */
NX_BRANDING_API QString mobileClientAndroidCompatibilityPackage();

/** Id of the old ios mobile application. */
NX_BRANDING_API QString mobileClientIosCompatibilityPackage();

/** Internal name, used as a key in settings storage, data folder, etc. */
NX_BRANDING_API QString traytoolInternalName();

/** Internal name, used as a key in settings storage, data folder, etc. */
NX_BRANDING_API QString mediaserverInternalName();

/** Name of the mediaserver service. */
NX_BRANDING_API QString mediaserverServiceName();

/** Base name of the distribution packages. */
NX_BRANDING_API QString installerName();

/** Custom desktop client, created using open source. */
NX_BRANDING_API bool isDesktopClientCustomized();

/** Mobile Client is enabled in customization. */
NX_BRANDING_API bool isMobileClientEnabledInCustomization();

/** Whether Client auto-update should be enabled by default. */
NX_BRANDING_API bool clientAutoUpdateEnabledByDefault();

/** Unique string designating a certain vendor-modified Client. */
NX_BRANDING_API QString customClientVariant();

/** URL referencing the releases.json file of the update server. */
NX_BRANDING_API QString customReleaseListUrl();

/** Offline update generator URL. */
NX_BRANDING_API QString customOfflineUpdateGeneratorUrl();

/** Custom url for information about the open source libraries. */
NX_BRANDING_API QString customOpenSourceLibrariesUrl();

} // namespace nx::branding
