#include "two_step_file_dialog.h"
#include "ui_two_step_file_dialog.h"

QnTwoStepFileDialog::QnTwoStepFileDialog(QWidget *parent, const QString &caption, const QString &directory, const QString &filter) :
    base_type(parent),
    ui(new Ui::QnTwoStepFileDialog)
{
    ui->setupUi(this);
}

QnTwoStepFileDialog::~QnTwoStepFileDialog() { }

void QnTwoStepFileDialog::setOption(QFileDialog::Option option, bool on) {
    dialog->setOption(option, on); //TODO: #GDM handle some options locally?
}

void QnTwoStepFileDialog::setOptions(QFileDialog::Options options) {
    dialog->setOptions(options);
}

void QnTwoStepFileDialog::setFileMode(QFileDialog::FileMode mode) {
    dialog->setFileMode(mode);
}

QFileDialog::FileMode QnTwoStepFileDialog::fileMode() const {
    return dialog->fileMode();
}

void QnTwoStepFileDialog::setAcceptMode(QFileDialog::AcceptMode mode) {
    dialog->setAcceptMode(mode);
}

QFileDialog::AcceptMode QnTwoStepFileDialog::acceptMode() const {
    return dialog->acceptMode();
}

QString QnTwoStepFileDialog::selectedFile() const {
    return QString(); //TODO: #GDM
}

QString QnTwoStepFileDialog::selectedNameFilter() const {
    return QString();
}

QGridLayout* QnTwoStepFileDialog::customizedLayout() const {
    return ui->optionsLayout;
}
