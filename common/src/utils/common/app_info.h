#pragma once

#include <QtCore/QString>

struct QnAppInfo
{
    static int ec2ProtoVersion();

    static QString organizationName();
    static QString linuxOrganizationName();
    static QString realm();

    static QString applicationVersion();
    static QString applicationRevision();
    static QString applicationPlatform();

    static QString applicationArch();
    static QString applicationPlatformModification();
    static QString applicationCompiler();

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

    static QString showcaseUrl();               //#GDM #FIXME will not work in 3.0
    static QString settingsUrl();               //#GDM #FIXME will not work in 3.0
    static QString helpUrl();                   //#GDM #FIXME will not work in 3.0

    static QString updateGeneratorUrl();
    static QString defaultCloudPortalUrl();
    static QString defaultCloudModulesXmlUrl();
    static QString cloudName();

    static int     freeLicenseCount();
    static QString freeLicenseKey();
    static bool    freeLicenseIsTrial();

    /** Folder where application is installed normally. */
    static QString installationRoot();

    // helpers:

    inline
    static QString applicationFullVersion()
    {
        static const QString kFullVersionTemplate(QLatin1String("%1-%2-%3%4"));

        // TODO: static const when VS supports c++11
        return kFullVersionTemplate
                .arg(applicationVersion())
                .arg(applicationRevision())
                .arg(customizationName().replace(QLatin1Char(' '), QLatin1Char('_')))
                .arg(QLatin1String(beta() ? "-beta" : ""));
    }
};

