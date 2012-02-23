#include "layout_save_dialog.h"
#include "ui_layout_save_dialog.h"

QnLayoutSaveDialog::QnLayoutSaveDialog(QWidget *parent):
    QDialog(parent),
    ui(new Ui::LayoutSaveDialog()),
    m_clickedButton(QDialogButtonBox::Cancel)
{
    ui->setupUi(this);

    m_labelText = ui->label->text();

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(at_buttonBox_clicked(QAbstractButton *)));
}

QnLayoutSaveDialog::~QnLayoutSaveDialog() {
    return;
}

QString QnLayoutSaveDialog::layoutName() const {
    return ui->nameLineEdit->text();
}

void QnLayoutSaveDialog::setLayoutName(const QString &layoutName) {
    ui->label->setText(m_labelText.arg(layoutName));
    ui->nameLineEdit->setText(layoutName);
}

void QnLayoutSaveDialog::at_buttonBox_clicked(QAbstractButton *button) {
    m_clickedButton = ui->buttonBox->standardButton(button);
}
