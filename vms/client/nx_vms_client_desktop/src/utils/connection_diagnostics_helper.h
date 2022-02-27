// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/software_version.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx_ec/ec_api_fwd.h>

namespace nx::vms::api {

class SoftwareVersion;
struct ModuleInformation;

} // namespace nx::vms::api

namespace nx::vms::client::core { struct ConnectionInfo; }

class QnWorkbenchContext;

class QnConnectionDiagnosticsHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;
    using RemoteConnectionError = nx::vms::client::core::RemoteConnectionError;

public:
    [[nodiscard]]
    static bool downloadAndRunCompatibleVersion(
        QWidget* parentWidget,
        const nx::vms::api::ModuleInformation& moduleInformation,
        nx::vms::client::core::ConnectionInfo connectionInfo,
        const nx::vms::api::SoftwareVersion& engineVersion);

    /**
     * Show message box with connection error description. If parent widget is not passed, main
     * window will be used as a parent window.
     */
    static void showConnectionErrorMessage(
        QnWorkbenchContext* context,
        RemoteConnectionError errorCode,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::vms::api::SoftwareVersion& engineVersion,
        QWidget* parentWidget = nullptr);

    static void failedRestartClientMessage(QWidget* parent);

    static QString developerModeText(
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::vms::api::SoftwareVersion& engineVersion);

private:
    static bool getInstalledVersions(QList<nx::utils::SoftwareVersion>* versions);
    /** Pass cloud password when connecting to an older (up to 4.2) system. */
    static void fixConnectionInfo(
        const nx::vms::api::ModuleInformation& moduleInformation,
        nx::vms::client::core::ConnectionInfo* connectionInfo);
};
