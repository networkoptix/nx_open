#include "reconnect_info_dialog.h"
#include "ui_reconnect_info_dialog.h"

QnReconnectInfoDialog::QnReconnectInfoDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::ReconnectInfoDialog()),
    m_cancelled(false)
{
    ui->setupUi(this);

    connect(ui->buttonBox,  &QDialogButtonBox::rejected, this, [this]{
        m_cancelled = true;
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
        ui->label->setText(tr("Canceling..."));
    });
}

QnReconnectInfoDialog::~QnReconnectInfoDialog()
{}

QString QnReconnectInfoDialog::text() const {
    return ui->infoLabel->text();
}

void QnReconnectInfoDialog::setText(const QString &text) {
    ui->infoLabel->setText(text);
}

bool QnReconnectInfoDialog::wasCanceled() const {
    return m_cancelled;
}
