#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui { class MultipleLayoutSelectionDialog; }

namespace nx {
namespace client {
namespace desktop {

class MultipleLayoutSelectionDialog : public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    MultipleLayoutSelectionDialog(QWidget *parent = nullptr);

    virtual ~MultipleLayoutSelectionDialog() override;

private:
    struct Private;
    const QScopedPointer<Private> d;
    const QScopedPointer<Ui::MultipleLayoutSelectionDialog> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

