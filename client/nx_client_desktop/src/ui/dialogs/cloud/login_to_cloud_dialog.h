#pragma once

#include <ui/dialogs/common/dialog.h>

namespace Ui {
    class QnLoginToCloudDialog;
} // namespace Ui

class QnLoginToCloudDialogPrivate;

class QnLoginToCloudDialog : public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    explicit QnLoginToCloudDialog(QWidget* parent);
    virtual ~QnLoginToCloudDialog();

    void setLogin(const QString& login);

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    QScopedPointer<Ui::QnLoginToCloudDialog> ui;
    QScopedPointer<QnLoginToCloudDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnLoginToCloudDialog)
    Q_DISABLE_COPY(QnLoginToCloudDialog)
};
