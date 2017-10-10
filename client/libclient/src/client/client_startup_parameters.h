#pragma once

#include <utils/common/software_version.h>

#include <nx/utils/uuid.h>
#include <nx/vms/utils/system_uri.h>

struct QnStartupParameters
{
    enum { kInvalidScreen = -1 };

    QnStartupParameters();

    static QnStartupParameters fromCommandLineArg(int argc, char **argv);

    static QString createAuthenticationString(const QUrl& url,
        const QnSoftwareVersion& version = QnSoftwareVersion());

    static QUrl parseAuthenticationString(QString string);

    QUrl parseAuthenticationString() const;

    int screen;
    const static QString kScreenKey;

    bool allowMultipleClientInstances;
    const static QString kAllowMultipleClientInstancesKey;

    bool skipMediaFolderScan;
    bool ignoreVersionMismatch;
    bool vsyncDisabled;
    bool clientUpdateDisabled;
    bool softwareYuv;
    bool forceLocalSettings;
    bool fullScreenDisabled;
    bool showFullInfo;
    bool exportedMode;  /*< Client was run from an exported video exe-file. */
    bool hiDpiDisabled = false;
    bool profilerMode = false;

    bool selfUpdateMode;
    const static QString kSelfUpdateKey;

    QString devModeKey;
    QString authenticationString;
    QString delayedDrop;
    QString instantDrop;
    QString logLevel;
    QString ec2TranLogLevel;
    QString dynamicTranslationPath;
    QString lightMode;
    QnUuid videoWallGuid;
    QnUuid videoWallItemGuid;
    QString engineVersion;
    QString dynamicCustomizationPath;
    QString ipVersion;

    /** Uri when the client was launched as uri handler. */
    nx::vms::utils::SystemUri customUri;

    QString enforceSocketType;
    QString enforceMediatorEndpoint;

    QStringList files; //< File paths passed to the client.
};
