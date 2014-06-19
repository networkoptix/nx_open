#include "join_other_system_dialog.h"
#include "ui_join_other_system_dialog.h"

#include <QtCore/QUrl>

QnJoinOtherSystemDialog::QnJoinOtherSystemDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnJoinOtherSystemDialog)
{
    ui->setupUi(this);
}

QnJoinOtherSystemDialog::~QnJoinOtherSystemDialog() {}

QUrl QnJoinOtherSystemDialog::url() const {
    return QUrl::fromUserInput(ui->urlEdit->text());
}

QString QnJoinOtherSystemDialog::password() const {
    return ui->passwordEdit->text();
}
