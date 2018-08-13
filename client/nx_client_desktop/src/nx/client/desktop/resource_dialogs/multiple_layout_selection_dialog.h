#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class MultipleLayoutSelectionDialog; }

namespace nx {
namespace client {
namespace desktop {

class MultipleLayoutSelectionDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    static bool selectLayouts(
        QnLayoutResourceList& checkedLayouts,
        QWidget* parent);

private:
    MultipleLayoutSelectionDialog(
        const QnResourceList& checkedLayouts,
        QWidget* parent = nullptr);

    virtual ~MultipleLayoutSelectionDialog() override;

private:
    const QScopedPointer<Ui::MultipleLayoutSelectionDialog> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

