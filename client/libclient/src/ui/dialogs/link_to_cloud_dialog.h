#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui
{
    class QnLinkToCloudDialog;
}

class QnLinkToCloudDialogPrivate;

class QnLinkToCloudDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnLinkToCloudDialog(QWidget* parent = nullptr);
    ~QnLinkToCloudDialog();

    void accept() override;

private:
    QScopedPointer<Ui::QnLinkToCloudDialog> ui;
    QScopedPointer<QnLinkToCloudDialogPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QnLinkToCloudDialog)
    Q_DISABLE_COPY(QnLinkToCloudDialog)
};
