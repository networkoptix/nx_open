#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

namespace Ui
{
    class QnLinkToCloudDialog;
}

class QnLinkToCloudDialogPrivate;

class QnLinkToCloudDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnLinkToCloudDialog(QWidget *parent = 0);
    ~QnLinkToCloudDialog();

    void accept() override;

private:
    QScopedPointer<Ui::QnLinkToCloudDialog> ui;
    QScopedPointer<QnLinkToCloudDialogPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QnLinkToCloudDialog)
    Q_DISABLE_COPY(QnLinkToCloudDialog)
};
