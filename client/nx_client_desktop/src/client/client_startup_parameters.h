#pragma once

#include <utils/common/software_version.h>

#include <nx/utils/uuid.h>
#include <nx/vms/utils/system_uri.h>

struct QnStartupParameters
{
    static const int kInvalidScreen = -1;
    static const bool kFullScreenDisabledByDefault =
#if defined Q_OS_MAC
        true;
#else
        false;
#endif

    static QnStartupParameters fromCommandLineArg(int argc, char **argv);

    static QString createAuthenticationString(const QUrl& url,
        const QnSoftwareVersion& version = QnSoftwareVersion());

    static QUrl parseAuthenticationString(QString string);

    QUrl parseAuthenticationString() const;

    bool isDevMode() const;

    int screen = kInvalidScreen;
    const static QString kScreenKey;

    bool allowMultipleClientInstances = false;
    const static QString kAllowMultipleClientInstancesKey;

    bool skipMediaFolderScan = false;
    bool ignoreVersionMismatch = false;
    bool vsyncDisabled = false;
    bool clientUpdateDisabled = false;
    bool softwareYuv = false;
    bool forceLocalSettings = false;
    bool fullScreenDisabled = kFullScreenDisabledByDefault;
    bool showFullInfo = false;
    bool exportedMode = false;  /*< Client was run from an exported video exe-file. */
    bool hiDpiDisabled = false;
    bool profilerMode = false;

    bool selfUpdateMode = false;
    const static QString kSelfUpdateKey;

    QString devModeKey;
    QString authenticationString;
    QString delayedDrop;
    QString instantDrop;
    QString logLevel;
    QString ec2TranLogLevel;
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
