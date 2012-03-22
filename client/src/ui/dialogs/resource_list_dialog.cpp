#include "resource_list_dialog.h"

#include <QtGui/QLabel>
#include <QtGui/QTableView>
#include <QtGui/QVBoxLayout>

#include <ui/models/resource_list_model.h>

#include "ui_resource_list_dialog.h"

QnResourceListDialog::QnResourceListDialog(QWidget *parent):
    QnButtonBoxDialog(parent),
    ui(new Ui::ResourceListDialog())
{
    ui->setupUi(this);

    m_model = new QnResourceListModel(this);

    ui->treeView->setModel(m_model);
    ui->topLabel->hide();
    ui->bottomLabel->hide();

    setButtonBox(ui->buttonBox);
}

QnResourceListDialog::~QnResourceListDialog() {
    return;
}

bool QnResourceListDialog::isReadOnly() const {
    return m_model->isReadOnly();
}

void QnResourceListDialog::setReadOnly(bool readOnly) {
    m_model->setReadOnly(readOnly);
}

void QnResourceListDialog::setText(const QString &text) {
    ui->topLabel->setText(text);

    ui->topLabel->setVisible(!text.isEmpty());
}

QString QnResourceListDialog::text() const {
    return ui->topLabel->text();
}

void QnResourceListDialog::setBottomText(const QString &bottomText) {
    ui->bottomLabel->setText(bottomText);

    ui->bottomLabel->setVisible(!bottomText.isEmpty());
}

QString QnResourceListDialog::bottomText() const {
    return ui->bottomLabel->text();
}

void QnResourceListDialog::setStandardButtons(QDialogButtonBox::StandardButtons standardButtons) {
    ui->buttonBox->setStandardButtons(standardButtons);
}

QDialogButtonBox::StandardButtons QnResourceListDialog::standardButtons() const {
    return ui->buttonBox->standardButtons();
}

void QnResourceListDialog::setResources(const QnResourceList &resources) {
    m_model->setResources(resources);
}

const QnResourceList &QnResourceListDialog::resources() const {
    return m_model->resouces();
}

void QnResourceListDialog::accept() {
    if(!isReadOnly())
        m_model->submitToResources();

    QDialog::accept();
}

QDialogButtonBox::StandardButton QnResourceListDialog::exec(QWidget *parent, const QnResourceList &resources, const QString &title, const QString &text, const QString &bottomText, QDialogButtonBox::StandardButtons buttons, bool readOnly) {
    QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(parent));
    dialog->setResources(resources);
    dialog->setWindowTitle(title);
    dialog->setText(text);
    dialog->setBottomText(bottomText);
    dialog->setStandardButtons(buttons);
    dialog->setReadOnly(readOnly);
    dialog->exec();
    return dialog->clickedButton();
}

QDialogButtonBox::StandardButton QnResourceListDialog::exec(QWidget *parent, const QnResourceList &resources, const QString &title, const QString &text, QDialogButtonBox::StandardButtons buttons, bool readOnly) {
    return exec(parent, resources, title, text, QString(), buttons, readOnly);
}



