#include "resource_list_dialog.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include <ui/models/resource_list_model.h>
#include <ui/help/help_topic_accessor.h>

#include "ui_resource_list_dialog.h"

QnResourceListDialog::QnResourceListDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::ResourceListDialog())
{
    ui->setupUi(this);

    m_model = new QnResourceListModel(this);

    ui->treeView->setModel(m_model);
    ui->topLabel->hide();
    ui->bottomLabel->hide();
    ui->doNotAskAgainCheckBox->hide();
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

void QnResourceListDialog::showIgnoreCheckbox() {
    ui->doNotAskAgainCheckBox->show();
}

bool QnResourceListDialog::isIgnoreCheckboxChecked() const {
    return ui->doNotAskAgainCheckBox->isChecked();
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

QDialogButtonBox::StandardButton QnResourceListDialog::exec(QWidget *parent, const QnResourceList &resources, int helpTopicId, const QString &title, const QString &text, const QString &bottomText, QDialogButtonBox::StandardButtons buttons, bool readOnly) {
    QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(parent));
    dialog->setResources(resources);
    dialog->setWindowTitle(title);
    dialog->setText(text);
    dialog->setBottomText(bottomText);
    dialog->setStandardButtons(buttons);
    dialog->setReadOnly(readOnly);
    setHelpTopic(dialog.data(), helpTopicId);
    dialog->exec();
    return dialog->clickedButton();
}

QDialogButtonBox::StandardButton QnResourceListDialog::exec(QWidget *parent, const QnResourceList &resources, const QString &title, const QString &text, QDialogButtonBox::StandardButtons buttons, bool readOnly) {
    return exec(parent, resources, -1, title, text, QString(), buttons, readOnly);
}

QDialogButtonBox::StandardButton QnResourceListDialog::exec(QWidget *parent, const QnResourceList &resources, const QString &title, const QString &text, const QString &bottomText, QDialogButtonBox::StandardButtons buttons, bool readOnly) {
    return exec(parent, resources, -1, title, text, bottomText, buttons, readOnly);
}

QDialogButtonBox::StandardButton QnResourceListDialog::exec(QWidget *parent, const QnResourceList &resources, int helpTopicId, const QString &title, const QString &text, QDialogButtonBox::StandardButtons buttons, bool readOnly) {
    return exec(parent, resources, helpTopicId, title, text, QString(), buttons, readOnly);
}

