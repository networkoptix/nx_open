#include "join_other_system_dialog.h"
#include "ui_join_other_system_dialog.h"

QnJoinOtherSystemDialog::QnJoinOtherSystemDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnJoinOtherSystemDialog)
{
    ui->setupUi(this);
}

QnJoinOtherSystemDialog::~QnJoinOtherSystemDialog() {}

QUrl QnJoinOtherSystemDialog::url() const {
    return QUrl(ui->urlEdit->text());
}

QString QnJoinOtherSystemDialog::password() const {
    return ui->passwordEdit->text();
}
