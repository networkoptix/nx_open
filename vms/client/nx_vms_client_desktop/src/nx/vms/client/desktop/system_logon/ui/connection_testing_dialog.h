// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>
#include <QtWidgets/QDialog>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx_ec/ec_api_fwd.h>
#include <ui/dialogs/common/button_box_dialog.h>

namespace nx::vms::client::desktop {

class ConnectionTestingDialog: public QnButtonBoxDialog
{
    Q_OBJECT

public:
    explicit ConnectionTestingDialog(QWidget* parent = nullptr);
    virtual ~ConnectionTestingDialog();

    void testConnection(
        nx::network::SocketAddress address,
        nx::network::http::Credentials credentials,
        const nx::utils::SoftwareVersion& engineVersion);

signals:
    void connectRequested(nx::vms::client::core::RemoteConnectionPtr connection);
    void loginToCloudRequested();
    void setupNewServerRequested(nx::Uuid expectedServerId);

private:
    void tick();

    /**
     * Updates ui elements depending of the test result.
     */
    void updateUi();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
