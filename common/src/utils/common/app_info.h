#pragma once

#include <QtCore/QString>

class QnAppInfo: public QObject
{
    Q_OBJECT

public:
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
    static QString defaultCloudHost();
    static QString defaultCloudPortalUrl();
    static QString defaultCloudModulesXmlUrl();
    static Q_INVOKABLE QString cloudName();

    static int     freeLicenseCount();
    static QString freeLicenseKey();
    static bool    freeLicenseIsTrial();

    static bool isArm() { return applicationArch() == QLatin1String("arm"); }
    static bool isBpi() { return armBox() == QLatin1String("bpi"); }
    static bool isNx1() { return armBox() == QLatin1String("nx1"); }

    static QString applicationFullVersion()
    {
        static const QString kFullVersion = QString(QLatin1String("%1-%2-%3%4"))
            .arg(applicationVersion())
            .arg(applicationRevision())
            .arg(customizationName().replace(QLatin1Char(' '), QLatin1Char('_')))
            .arg(QLatin1String(beta() ? "-beta" : ""));

        return kFullVersion;
    }
};

