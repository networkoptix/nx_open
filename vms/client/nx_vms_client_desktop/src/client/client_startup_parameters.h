// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/logon_data.h>
#include <nx/vms/utils/system_uri.h>

struct QnStartupParameters
{
    static const int kInvalidScreen = -1;
    static const bool kFullScreenDisabledByDefault = false;

    static QnStartupParameters fromCommandLineArg(int argc, char **argv);

    static QString createAuthenticationString(
        const nx::vms::client::core::LogonData& logonData,
        std::optional<nx::utils::SoftwareVersion> version = std::nullopt);

    static nx::vms::client::core::LogonData parseAuthenticationString(QString string);
    nx::vms::client::core::LogonData parseAuthenticationString() const;

    bool isVideoWallMode() const;

    bool allowMultipleClientInstances = false;
    const static QString kAllowMultipleClientInstancesKey;

    bool skipMediaFolderScan = false;
    bool clientUpdateDisabled = false;
    bool softwareYuv = false;
    bool forceLocalSettings = false;
    bool showFullInfo = false;
    bool exportedMode = false;  /*< Client was run from an exported video exe-file. */

    bool selfUpdateMode = false;
    const static QString kSelfUpdateKey;

    /**
     * Special mode for ACS and similar scenarios: client does not have any UI elements, opens
     * fixed cameras and limits timeline by 5-min window.
     */
    bool acsMode = false;

    QString authenticationString;
    const static QString kAuthenticationStringKey;

    QString sessionId;
    const static QString kSessionIdKey;

    QString stateFileName;
    const static QString kStateFileKey;

    int screen = kInvalidScreen;
    const static QString kScreenKey;

    bool fullScreenDisabled = kFullScreenDisabledByDefault;
    const static QString kFullScreenDisabledKey;

    QRect windowGeometry;
    const static QString kWindowGeometryKey;

    bool skipAutoLogin = false;
    const static QString kSkipAutoLoginKey;

    QString delayedDrop;
    QString instantDrop;
    QString layoutRef;
    QString logLevel;
    QString logFile;
    QString lightMode;
    QnUuid videoWallGuid;
    QnUuid videoWallItemGuid;
    QString engineVersion;
    int vmsProtocolVersion = 0;
    QString ipVersion;
    QString scriptFile;
    QString traceFile;

    /** Uri when the client was launched as uri handler. */
    nx::vms::utils::SystemUri customUri;

    QString enforceSocketType;
    QString enforceMediatorEndpoint;

    QStringList files; //< File paths passed to the client.

    /**
     * Debug parameter to develop QML modules without client restart. Should contain absolute path
     * to the /open/vms/client/nx_vms_client_desktop/qml folder.
     */
    QString qmlRoot;

    /**
     * Debug parameter to develop QML modules without client restart. Should contain absolute path
     * to the /open/vms/client/nx_vms_client_core/static-resources/qml folder.
     */
    QStringList qmlImportPaths;
};
