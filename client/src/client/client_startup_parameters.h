
#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/utils/system_uri.h>

struct QnStartupParameters
{
    enum { kInvalidScreen = -1 };

    QnStartupParameters();

    static QnStartupParameters fromCommandLineArg(int argc
        , char **argv);

    int screen;

    bool allowMultipleClientInstances;
    bool skipMediaFolderScan;
    bool ignoreVersionMismatch;
    bool vsyncDisabled;
    bool clientUpdateDisabled;
    bool softwareYuv;
    bool forceLocalSettings;
    bool fullScreenDisabled;
    bool showFullInfo;

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

    /** Uri when the client was launched as uri handler. */
    nx::vms::utils::SystemUri customUri;

    QString enforceSocketType;
    QString enforceMediatorEndpoint;
};
