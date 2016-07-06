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

QString QnAppInfo::realm()
{
    return QStringLiteral("VMS");
}

QString QnAppInfo::applicationVersion()
{
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString QnAppInfo::applicationRevision()
{
    return QStringLiteral("${changeSet}");
}

QString QnAppInfo::applicationPlatform()
{
    return QStringLiteral("${platform}");
}

QString QnAppInfo::applicationArch()
{
    return QStringLiteral("${arch}");
}

QString QnAppInfo::applicationPlatformModification()
 {
    return QStringLiteral("${modification}");
}

QString QnAppInfo::applicationCompiler()
 {
    return QStringLiteral("${additional.compiler}");
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

QString QnAppInfo::armBox()
{
    //#ak for now box has sense value on ARM devices only.
        //On other platforms it is used by build system for internal purposes
#ifdef __arm__
    return QStringLiteral("${box}");
#else
    return QString();
#endif
}

bool QnAppInfo::beta()
{
    return ${beta};
}

QString QnAppInfo::productName()
{
#ifdef _WIN32
    return QStringLiteral("${product.name}");
#else
    return QStringLiteral("${namespace.additional}");
#endif
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
    return QStringLiteral("${translation1}");
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
    return QStringLiteral("${company.license.address}");
}

QString QnAppInfo::companyUrl()
{
    return QStringLiteral("${company.url}");
}

QString QnAppInfo::supportEmailAddress()
{
    return QStringLiteral("${company.support.address}");
}

QString QnAppInfo::supportLink()
{
    return QStringLiteral("${company.support.link}");
}

QString QnAppInfo::showcaseUrl()
{
    return QStringLiteral("${showcase.url}/${customization}");
}

QString QnAppInfo::settingsUrl()
{
    return QStringLiteral("${settings.url}/${customization}.json");
}

QString QnAppInfo::helpUrl()
{
    return QStringLiteral("${helpUrl}/${customization}/${parsedVersion.majorVersion}/${parsedVersion.minorVersion}/url");
}

QString QnAppInfo::updateGeneratorUrl()
{
    return QStringLiteral("${update.generator.url}");
}

QString QnAppInfo::defaultCloudHost()
{
    return QStringLiteral("${cloud.host}");
}

QString QnAppInfo::defaultCloudPortalUrl()
{
    return QStringLiteral("${cloud.portalUrl}");
}

QString QnAppInfo::defaultCloudModulesXmlUrl()
{
    return QStringLiteral("${cloud.modulesXmlUrl}");
}

QString QnAppInfo::cloudName()
{
    return QStringLiteral("${cloud.name}");
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

QString QnAppInfo::installationRoot()
{
    return QStringLiteral("${installation.root}");
}

