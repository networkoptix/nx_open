#pragma once

#include <ui/dialogs/common/dialog.h>

namespace Ui
{
    class QnLoginToCloudDialog;
}

class QnLoginToCloudDialogPrivate;

class QnLoginToCloudDialog : public QnDialog
{
    Q_OBJECT

    typedef QnDialog base_type;

public:
    explicit QnLoginToCloudDialog(QWidget *parent = nullptr);
    ~QnLoginToCloudDialog();

    void setLogin(const QString &login);

private:
    QScopedPointer<Ui::QnLoginToCloudDialog> ui;
    QScopedPointer<QnLoginToCloudDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnLoginToCloudDialog)
    Q_DISABLE_COPY(QnLoginToCloudDialog)
};
