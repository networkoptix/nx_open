#pragma once

#include <core/resource/resource_fwd.h>

#include <client_core/connection_context_aware.h>

#include <nx/vms/client/desktop/common/dialogs/generic_tabbed_dialog.h>

namespace Ui { class LayoutSettingsDialog; }

namespace nx::vms::client::desktop {

struct LayoutSettingsDialogState;

class LayoutSettingsDialog:
    public GenericTabbedDialog,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = GenericTabbedDialog;

public:
    explicit LayoutSettingsDialog(QWidget* parent = nullptr);
    virtual ~LayoutSettingsDialog() override;

    bool setLayout(const QnLayoutResourcePtr& layout);

    virtual void accept() override;

private:
    void loadState(const LayoutSettingsDialogState& state);

private:
    Q_DISABLE_COPY(LayoutSettingsDialog)
    QScopedPointer<Ui::LayoutSettingsDialog> ui;

    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
