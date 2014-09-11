#ifndef QN_APPINFO_H
#define QN_APPINFO_H

#include <QtCore/QString>

struct QnAppInfo {
    static QString organizationName();
    static QString linuxOrganizationName();

    static QString applicationName();
    static QString applicationVersion();
    static QString applicationRevision();
    static QString applicationPlatform();
    static QString applicationArch();
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
    
    static QString clientExecutableName();
    static QString applauncherExecutableName();
    
    static QString mediaFolderName();
    
    static QString licensingEmailAddress();
    static QString companyUrl();
    static QString supportAddress();
    static QString showcaseUrl();
    static QString settingsUrl();
    static QString mirrorListUrl();
    static QString helpUrl();

    static int     freeLicenseCount();
    static QString freeLicenseKey();
    static bool    freeLicenseIsTrial();

    static QString iosPlayButtonTint();
};

#endif // QN_APPINFO_H
