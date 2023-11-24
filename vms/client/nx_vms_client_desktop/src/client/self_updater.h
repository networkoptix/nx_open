// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>

#include <nx/utils/software_version.h>

struct QnStartupParameters;

namespace nx::vms::client::desktop {

/**
    Class for updating client installation if needed.

    There are some operations that should be done during self-update.
    Several of them require administrator privileges on different os.

    | Operation              | Windows | Linux   | MacOS   |
    | ---------------------- | ------- | ------- | ------- |
    | Register URI           | admin   | user    | ???     |
    | Update applaucher      | user    | user    | user    |
    | Update minilauncher    | admin   | admin   | ???     |
    | Update quickstartguide | admin   | -----   | ---     |
    | ---------------------- | ------- | ------- | ------- |

    If client is run under administrator (with self-update key passed),
    we should do all required operations and then exit immediately.
*/
class SelfUpdater
{
public:
    SelfUpdater(const QnStartupParameters& startupParams);

    static bool runMinilaucher();
    static bool runNewClient(const QStringList& args);

private:
    struct UriHandlerUpdateResult
    {
        bool success = false;
        bool upgrade = false;
    };

    struct HelpFileDescription
    {
        QString fileName;
        QString shortcutName;
        QString helpName;
        QString componentDataDirName;
    };

    UriHandlerUpdateResult registerUriHandler();
    bool registerNovFilesAssociation();
    void updateApplauncher();
    bool updateMinilauncherOnWindows(bool hasAdminRights);
    bool updateHelpFile(const HelpFileDescription& helpDescription);

    nx::utils::SoftwareVersion getVersionFromFile(const QString& filename) const;
    bool saveVersionToFile(const QString& filename, const nx::utils::SoftwareVersion& version) const;

    void relaunchSelfUpdaterWithAdminPermissionsOnWindows();

    bool isMinilaucherUpdated(const QDir& installRoot) const;

    bool updateMinilauncherOnWindowsInDir(const QDir& installRoot, const QString& sourceMinilauncherPath);
    bool updateApplauncherDesktopIcon();

    void updateMinilauncherIconsOnWindows(bool hasAdminRights);

private:
    nx::utils::SoftwareVersion m_clientVersion;
};

} // namespace nx::vms::client::desktop
