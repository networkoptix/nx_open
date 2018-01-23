//
// This file is generated. Go to pom.xml.
//
#include <utils/common/app_info.h>

int QnAppInfo::ec2ProtoVersion()
{
    return ${nxec.ec2ProtoVersion};
}

QString QnAppInfo::organizationName()
{
    return QStringLiteral("${company.name}");
}

QString QnAppInfo::linuxOrganizationName()
{
    return QStringLiteral("${deb.customization.company.name}");
}

QString QnAppInfo::applicationVersion()
{
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString QnAppInfo::applicationRevision()
{
    return QStringLiteral("${changeSet}");
}

QString QnAppInfo::applicationPlatformModification()
 {
    return QStringLiteral("${modification}");
}

QString QnAppInfo::applicationCompiler()
{
    #if defined(__clang__)
        return QStringLiteral("clang");
    #elif defined(__GNUC__)
        return QStringLiteral("gcc");
    #elif defined(_MSC_VER)
        return QStringLiteral("msvc");
    #else
        return QStringLiteral();
    #endif
}

QString QnAppInfo::engineVersion()
 {
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString QnAppInfo::ffmpegVersion()
 {
    return QStringLiteral("${ffmpeg.version}");
}

QString QnAppInfo::sigarVersion()
{
    return QStringLiteral("${sigar.version}");
}

QString QnAppInfo::boostVersion()
{
    return QStringLiteral("${boost.version}");
}

bool QnAppInfo::beta()
{
    return ${beta};
}

QString QnAppInfo::productNameShort()
{
    return QStringLiteral("${product.name.short}");
}

QString QnAppInfo::productNameLong()
{
    return QStringLiteral("${display.product.name}");
}

QString QnAppInfo::customizationName()
{
    return QStringLiteral("${customization}");
}

QString QnAppInfo::defaultLanguage()
{
    return QStringLiteral("${defaultTranslation}");
}

QString QnAppInfo::clientExecutableName()
{
    return QStringLiteral("${client.binary.name}");
}

QString QnAppInfo::applauncherExecutableName()
{
    return QStringLiteral("${applauncher.binary.name}");
}

QString QnAppInfo::mediaFolderName()
{
    return QStringLiteral("${client.mediafolder.name}");
}

QString QnAppInfo::licensingEmailAddress()
{
    return QStringLiteral(R"(${licenseEmail})");
}

QString QnAppInfo::companyUrl()
{
    return QStringLiteral("${companyUrl}");
}

QString QnAppInfo::supportEmailAddress()
{
    return QStringLiteral("${supportEmail}");
}

QString QnAppInfo::supportUrl()
{
    return QStringLiteral("${supportUrl}");
}

QString QnAppInfo::supportPhone()
{
    return QStringLiteral("${supportPhone}");
}

QString QnAppInfo::updateGeneratorUrl()
{
    return QStringLiteral("${update.generator.url}");
}

int QnAppInfo::freeLicenseCount()
{
    return ${freeLicenseCount};
}

QString QnAppInfo::freeLicenseKey()
{
    return QStringLiteral("${freeLicenseKey}");
}

bool QnAppInfo::freeLicenseIsTrial()
{
    return ${freeLicenseIsTrial};
}
