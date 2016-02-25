#pragma once

#include <QtCore/QString>

struct QnAppInfo {
    static QString organizationName();
    static QString linuxOrganizationName();
    static QString realm();

    static QString applicationVersion();
    static QString applicationRevision();
    static QString applicationPlatform();

    static QString applicationArch();
    static QString applicationPlatformModification();
    static QString applicationCompiler();

    static QString applicationUriProtocol();

    static QString engineVersion();
    static QString ffmpegVersion();
    static QString sigarVersion();
    static QString boostVersion();
    static QString armBox();
    static bool    beta();

    static QString productName();
    static QString productNameShort();
    static QString productNameLong();
    static QString customizationName();

    static QString defaultLanguage();

    static QString clientExecutableName();
    static QString applauncherExecutableName();

    static QString mediaFolderName();

    static QString licensingEmailAddress();
    static QString companyUrl();

    static QString supportEmailAddress();
    static QString supportLink();

    static QString showcaseUrl();
    static QString settingsUrl();
    static QString mirrorListUrl();
    static QString helpUrl();
    static QString updateGeneratorUrl();
    static QString cloudPortalUrl();
    static QString cloudName();

    static int     freeLicenseCount();
    static QString freeLicenseKey();
    static bool    freeLicenseIsTrial();

    static QString iosPlayButtonTint();

    static QString oldAndroidClientLink();
    static QString oldIosClientLink();

    static QString oldAndroidAppId();

    // helpers:

    inline
    static QString applicationFullVersion()
    {
        // TODO: static const when VS supports c++11
        return QString(QLatin1String("%1-%2-%3%4"))
                .arg(applicationVersion())
                .arg(applicationRevision())
                .arg(customizationName().replace(QLatin1Char(' '), QLatin1Char('_')))
                .arg(QLatin1String(beta() ? "-beta" : ""));
    }
};

