#include "update_url_dialog.h"
#include "ui_update_url_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QPushButton>

QnUpdateUrlDialog::QnUpdateUrlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnUpdateUrlDialog)
{
    ui->setupUi(this);
    QPushButton *copyButton = ui->buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, this, &QnUpdateUrlDialog::at_copyButton_clicked);
}

QnUpdateUrlDialog::~QnUpdateUrlDialog() {}

QString QnUpdateUrlDialog::updatesUrl() const {
    return ui->urlLabel->text();
}

void QnUpdateUrlDialog::setUpdatesUrl(const QString &url) {
    ui->urlLabel->setText(url);
}

void QnUpdateUrlDialog::at_copyButton_clicked() {
    qApp->clipboard()->setText(ui->urlLabel->text());
}
