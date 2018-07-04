#pragma once

#include <core/resource/resource_fwd.h>
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
    static bool getLayouts(QWidget* parent, QnResourceList& resources);

private:
    MultipleLayoutSelectionDialog(QWidget *parent = nullptr);
    virtual ~MultipleLayoutSelectionDialog() override;

private:
    const QScopedPointer<Ui::MultipleLayoutSelectionDialog> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

