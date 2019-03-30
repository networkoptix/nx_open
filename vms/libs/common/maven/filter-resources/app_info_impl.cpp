//
// This file is generated.
//
#include <utils/common/app_info.h>

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
