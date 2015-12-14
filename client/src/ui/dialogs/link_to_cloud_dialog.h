#pragma once

#include <QtWidgets/QDialog>

namespace Ui
{
    class QnLinkToCloudDialog;
}

class QnLinkToCloudDialogPrivate;

class QnLinkToCloudDialog : public QDialog
{
    Q_OBJECT

    typedef QDialog base_type;

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
