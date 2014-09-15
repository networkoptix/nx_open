//
// This file is generated. Go to pom.xml.
//
#include <utils/common/app_info.h>

//unused variable, why was in required?
#define QN_BUILDENV_PATH                "${environment.dir}"

QString QnAppInfo::organizationName() {
    return QStringLiteral("${company.name}");
}

QString QnAppInfo::linuxOrganizationName() {
    return QStringLiteral("${deb.customization.company.name}");
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

QString QnAppInfo::applicationCompiler() {
    return QStringLiteral("${additional.compiler}");
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
    return QStringLiteral("${box}");
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
    return QStringLiteral("${product.name}");
}

QString QnAppInfo::customizationName() {
    return lit("${customization}");
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

QString QnAppInfo::supportAddress() {
    return QStringLiteral("${company.support.address}");
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
