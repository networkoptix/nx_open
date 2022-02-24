// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class ServerSelectionDialog; }

namespace nx::vms::client::desktop {

class ServerSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    using ServerFilter = std::function<bool(const QnMediaServerResourcePtr&)>;

    static bool selectServers(
        QnUuidSet& selectedServers,
        ServerFilter filterFunctor,
        const QString& infoMessage,
        QWidget* parent);

private:
    ServerSelectionDialog(
        const QnUuidSet& selectedServersIds,
        ServerFilter filterFunctor,
        const QString& infoMessage,
        QWidget* parent = nullptr);

    virtual ~ServerSelectionDialog() override;

    bool isEmpty() const;

private:
    const std::unique_ptr<Ui::ServerSelectionDialog> ui;
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
