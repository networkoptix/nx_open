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

QString QnAppInfo::productName()
{
#ifdef _WIN32
    return QStringLiteral("${product.name}");
#else
    return QStringLiteral("${product.appName}");
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

//Filling string constant with zeros to be able to change this constant in already-built binary
static const char* kCloudHostNameWithPrefix = "this_is_cloud_host_name ${cloudHost}\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
static const char* kCloudHostName = kCloudHostNameWithPrefix + sizeof("this_is_cloud_host_name");

QString QnAppInfo::defaultCloudHost()
{
    return QString::fromUtf8(kCloudHostName);
}

QString QnAppInfo::defaultCloudPortalUrl()
{
    return QString::fromLatin1("https://%1").arg(defaultCloudHost());
}

QString QnAppInfo::defaultCloudModulesXmlUrl()
{
    return QString::fromLatin1("http://%1/api/cloud_modules.xml").arg(defaultCloudHost());
}

QString QnAppInfo::cloudName()
{
    return QStringLiteral("${cloudName}");
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
