// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/credentials.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <ui/dialogs/common/dialog.h>

namespace nx::vms::client::desktop {

class SetupWizardDialogPrivate;

class SetupWizardDialog: public QnDialog
{
    Q_OBJECT

    using base_type = QnDialog;

public:
    explicit SetupWizardDialog(
        nx::network::SocketAddress address,
        const QnUuid& serverId,
        QWidget* parent = nullptr);
    ~SetupWizardDialog() override;

    virtual int exec() override;

    void loadPage();

    nx::vms::api::Credentials credentials() const;

    bool savePassword() const;

private:
    QScopedPointer<SetupWizardDialogPrivate> d;
};

} // namespace nx::vms::client::desktop
