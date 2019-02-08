#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_fwd.h>

namespace Ui { class MultipleLayoutSelectionDialog; }

namespace nx::vms::client::desktop {

class MultipleLayoutSelectionDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    static bool selectLayouts(
        QnUuidSet& selectedLayouts,
        QWidget* parent);

private:
    MultipleLayoutSelectionDialog(
        const QnUuidSet& selectedLayouts,
        QWidget* parent = nullptr);

    virtual ~MultipleLayoutSelectionDialog() override;

private:
    struct Private;
    const QScopedPointer<Private> d;
    const QScopedPointer<Ui::MultipleLayoutSelectionDialog> ui;
};

} // namespace nx::vms::client::desktop

