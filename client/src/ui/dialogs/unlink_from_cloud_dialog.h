#pragma once

#include <QtWidgets/QDialog>

namespace Ui
{
    class QnUnlinkFromCloudDialog;
}

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QDialog
{
    Q_OBJECT
    typedef QDialog base_type;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    void accept() override;

private:
    QScopedPointer<Ui::QnUnlinkFromCloudDialog> ui;
    QScopedPointer<QnUnlinkFromCloudDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
