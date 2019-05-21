#pragma once

#include <QtCore/QMetaType>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/software_version.h>
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

    static QString createAuthenticationString(const nx::utils::Url& url,
        const nx::vms::api::SoftwareVersion& version = {});

    static nx::utils::Url parseAuthenticationString(QString string);

    nx::utils::Url parseAuthenticationString() const;

    bool isDevMode() const;
    bool isVideoWallMode() const;

    int screen = kInvalidScreen;
    const static QString kScreenKey;

    bool allowMultipleClientInstances = false;
    const static QString kAllowMultipleClientInstancesKey;

    bool skipMediaFolderScan = false;
    bool clientUpdateDisabled = false;
    bool softwareYuv = false;
    bool forceLocalSettings = false;
    bool fullScreenDisabled = kFullScreenDisabledByDefault;
    bool showFullInfo = false;
    bool exportedMode = false;  /*< Client was run from an exported video exe-file. */
    bool hiDpiDisabled = false;

    bool selfUpdateMode = false;
    const static QString kSelfUpdateKey;

    /**
     * Special mode for ACS and similar scenarios: client does not have any UI elements, opens
     * fixed cameras and limits timeline by 5-min window.
     */
    bool acsMode = false;

    QString devModeKey;
    QString authenticationString;
    QString delayedDrop;
    QString instantDrop;
    QString layoutRef;
    QString logLevel;
    QString logFile;
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

    QString qmlRoot;
    QStringList qmlImportPaths;
};

Q_DECLARE_METATYPE(QnStartupParameters)
