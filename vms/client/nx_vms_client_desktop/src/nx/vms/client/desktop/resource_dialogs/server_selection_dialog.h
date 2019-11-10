#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_fwd.h>

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
    struct Private;
    const QScopedPointer<Private> d;
    const QScopedPointer<Ui::ServerSelectionDialog> ui;
};

} // namespace nx::vms::client::desktop
