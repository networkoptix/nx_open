//
// This file is generated. Go to pom.xml.
//
#include <utils/common/app_info.h>

QString QnAppInfo::organizationName() {
    return QStringLiteral("${company.name}");
}

QString QnAppInfo::linuxOrganizationName() {
    return QStringLiteral("${deb.customization.company.name}");
}

QString QnAppInfo::realm() {
    return QStringLiteral("VMS");
}

QString QnAppInfo::applicationVersion() {
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString QnAppInfo::applicationRevision() {
    return QStringLiteral("${changeSet}");
}

QString QnAppInfo::applicationPlatform() {
    return QStringLiteral("${platform}");
}

QString QnAppInfo::applicationArch() {
    return QStringLiteral("${arch}");
}

QString QnAppInfo::applicationPlatformModification() {
    return QStringLiteral("${modification}");
}

QString QnAppInfo::applicationCompiler() {
    return QStringLiteral("${additional.compiler}");
}

QString QnAppInfo::applicationUriProtocol() {
    return QStringLiteral("${uri.protocol}");
}

QString QnAppInfo::engineVersion() {
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString QnAppInfo::ffmpegVersion() {
    return QStringLiteral("${ffmpeg.version}");
}

QString QnAppInfo::sigarVersion() {
    return QStringLiteral("${sigar.version}");
}

QString QnAppInfo::boostVersion() {
    return QStringLiteral("${boost.version}");
}

QString QnAppInfo::armBox() {
    //#ak for now box has sense value on ARM devices only. 
        //On other platforms it is used by build system for internal purposes
#ifdef __arm__
    return QStringLiteral("${box}");
#else
    return QString();
#endif
}

bool QnAppInfo::beta() {
    return ${beta};
}

QString QnAppInfo::productName() {
#ifdef _WIN32
    return QStringLiteral("${product.name}");
#else
    return QStringLiteral("${namespace.additional}");
#endif
}

QString QnAppInfo::productNameShort() {
    return QStringLiteral("${product.name.short}");
}

QString QnAppInfo::productNameLong() {
    return QStringLiteral("${display.product.name}");
}

QString QnAppInfo::customizationName() {
    return QStringLiteral("${customization}");
}

QString QnAppInfo::defaultLanguage() {
    return QStringLiteral("${translation1}");
}

QString QnAppInfo::clientExecutableName() {
#ifdef _WIN32
    return QStringLiteral("${product.name}.exe"); // TODO: #Boris probably there exists a variable for this?
#else
    return QStringLiteral("client-bin");
#endif
}

QString QnAppInfo::applauncherExecutableName() {
#ifdef _WIN32
    return QStringLiteral("${product.name} Launcher.exe");
#else
    return QStringLiteral("applauncher-bin");
#endif
}

QString QnAppInfo::mediaFolderName() {
    return QStringLiteral("${client.mediafolder.name}");
}

QString QnAppInfo::licensingEmailAddress() {
    return QStringLiteral("${company.license.address}");
}

QString QnAppInfo::companyUrl() {
    return QStringLiteral("${company.url}");
}

QString QnAppInfo::supportEmailAddress() {
    return QStringLiteral("${company.support.address}");
}

QString QnAppInfo::supportLink() {
    return QStringLiteral("${company.support.link}");
}

QString QnAppInfo::showcaseUrl() {
    return QStringLiteral("${showcase.url}/${customization}");
}

QString QnAppInfo::settingsUrl() {
    return QStringLiteral("${settings.url}/${customization}.json");
}

QString QnAppInfo::mirrorListUrl() {
    return QStringLiteral("${mirrorListUrl}");
}

QString QnAppInfo::helpUrl() {
    return QStringLiteral("${helpUrl}/${customization}/${parsedVersion.majorVersion}/${parsedVersion.minorVersion}/url");
}

QString QnAppInfo::updateGeneratorUrl() {
    return QStringLiteral("${update.generator.url}");
}

QString QnAppInfo::cloudPortalUrl() {
    return QStringLiteral("${cloud.portalUrl}");
}

QString QnAppInfo::cloudName() {
    return QStringLiteral("${cloud.name}");
}

int QnAppInfo::freeLicenseCount() {
    return ${freeLicenseCount};
}

QString QnAppInfo::freeLicenseKey() {
    return QStringLiteral("${freeLicenseKey}");
}

bool QnAppInfo::freeLicenseIsTrial() {
    return ${freeLicenseIsTrial};
}

QString QnAppInfo::iosPlayButtonTint() {
    return QStringLiteral("${ios.playButton.tint}");
}

QString QnAppInfo::oldAndroidClientLink() {
    return QStringLiteral("https://play.google.com/store/apps/details?id=${namespace.major}.${namespace.minor}.${namespace.additional}");
}

QString QnAppInfo::oldIosClientLink() {
    return QStringLiteral("https://itunes.apple.com/ru/app/${ios.old_app_appstore_id}");
}

QString QnAppInfo::oldAndroidAppId() {
    return QStringLiteral("${old.android.packagename}");
}
