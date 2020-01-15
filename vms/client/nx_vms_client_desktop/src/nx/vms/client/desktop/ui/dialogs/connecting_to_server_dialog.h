#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/utils/impl_ptr.h>

namespace Ui { class ConnectingToServerDialog; }

namespace nx::vms::client::desktop {

class ConnectingToServerDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit ConnectingToServerDialog(
        QWidget* parent = nullptr,
        Qt::WindowFlags windowFlags = {Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint});

    virtual ~ConnectingToServerDialog() override;

    void setDisplayedServer(const QnMediaServerResourcePtr& server);

private:
    nx::utils::ImplPtr<Ui::ConnectingToServerDialog> ui;
};

} // namespace nx::vms::client::desktop
