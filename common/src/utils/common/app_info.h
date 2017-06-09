#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

class QnAppInfo: public QObject
{
    Q_OBJECT

public:
    QnAppInfo(QObject* parent = nullptr);

    static Q_INVOKABLE int ec2ProtoVersion();

    static Q_INVOKABLE QString organizationName();
    static Q_INVOKABLE QString linuxOrganizationName();
    static Q_INVOKABLE QString organizationNameForSettings();
    static Q_INVOKABLE QString realm();

    static Q_INVOKABLE QString applicationVersion();
    static Q_INVOKABLE QString applicationRevision();

    static Q_INVOKABLE QString applicationPlatform();
    static Q_INVOKABLE QString applicationArch();
    static Q_INVOKABLE QString applicationPlatformModification();
    static Q_INVOKABLE QString applicationCompiler();

    static Q_INVOKABLE QString engineVersion();
    static Q_INVOKABLE QString ffmpegVersion();
    static Q_INVOKABLE QString sigarVersion();
    static Q_INVOKABLE QString boostVersion();
    static Q_INVOKABLE QString armBox();
    static Q_INVOKABLE bool    beta();

    static Q_INVOKABLE QString productName();
    static Q_INVOKABLE QString productNameShort();
    static Q_INVOKABLE QString productNameLong();
    static Q_INVOKABLE QString customizationName();

    static Q_INVOKABLE QString defaultLanguage();

    static Q_INVOKABLE QString clientExecutableName();
    static Q_INVOKABLE QString applauncherExecutableName();

    static Q_INVOKABLE QString mediaFolderName();

    static Q_INVOKABLE QString licensingEmailAddress();
    static Q_INVOKABLE QString companyUrl();

    static Q_INVOKABLE QString supportEmailAddress();
    static Q_INVOKABLE QString supportUrl();
    static Q_INVOKABLE QString supportPhone();

    static Q_INVOKABLE QString updateGeneratorUrl();
    static Q_INVOKABLE QString defaultCloudHost();
    static Q_INVOKABLE QString defaultCloudPortalUrl();
    static Q_INVOKABLE QString defaultCloudModulesXmlUrl();
    static Q_INVOKABLE QString cloudName();
    static Q_INVOKABLE QStringList compatibleCloudHosts();

    static Q_INVOKABLE int freeLicenseCount();
    static Q_INVOKABLE QString freeLicenseKey();
    static Q_INVOKABLE bool freeLicenseIsTrial();

    static Q_INVOKABLE bool isArm();
    static Q_INVOKABLE bool isBpi();
    static Q_INVOKABLE bool isNx1();
    static Q_INVOKABLE bool isAndroid();
    static Q_INVOKABLE bool isIos();
    static Q_INVOKABLE bool isMobile();
};

