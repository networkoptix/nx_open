// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class ServerSelectionDialog; }

namespace nx::vms::client::desktop {

class ServerSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    using ServerFilter = std::function<bool(const QnMediaServerResourcePtr&)>;
    using SelectServerCallback = std::function<void(bool success, nx::Uuid selectedServer)>;

    static bool selectServers(
        UuidSet& selectedServers,
        ServerFilter filterFunctor,
        const QString& infoMessage,
        QWidget* parent);

    static void selectServer(
        nx::Uuid selectedServer,
        ServerFilter filterFunctor,
        const QString& infoMessage,
        SelectServerCallback callback,
        QWidget* parent);

private:
    ServerSelectionDialog(
        const UuidSet& selectedServersIds,
        ServerFilter filterFunctor,
        const QString& infoMessage,
        bool multiSelect,
        QWidget* parent = nullptr);

    virtual ~ServerSelectionDialog() override;

    bool isEmpty() const;

private:
    const std::unique_ptr<Ui::ServerSelectionDialog> ui;
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
