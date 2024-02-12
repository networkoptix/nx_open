// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>
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
        const nx::Uuid& serverId,
        QWidget* parent = nullptr);
    ~SetupWizardDialog() override;

    virtual int exec() override;

    void loadPage();

    /** Admin password for connection. */
    QString password() const;

private:
    QScopedPointer<SetupWizardDialogPrivate> d;
    const nx::network::SocketAddress m_address;
};

} // namespace nx::vms::client::desktop
