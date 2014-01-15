#include "two_step_file_dialog.h"
#include "ui_two_step_file_dialog.h"

QnTwoStepFileDialog::QnTwoStepFileDialog(QWidget *parent, const QString &caption, const QString &directory, const QString &filter) :
    base_type(parent),
    ui(new Ui::QnTwoStepFileDialog)
{
    ui->setupUi(this);
    setWindowTitle(caption);

    ui->directoryLabel->setText(directory);


}

QnTwoStepFileDialog::~QnTwoStepFileDialog() { }

void QnTwoStepFileDialog::setOptions(QFileDialog::Options options) {
    Q_ASSERT(options == 0);
}

void QnTwoStepFileDialog::setFileMode(QFileDialog::FileMode mode) {
    Q_ASSERT(mode == QFileDialog::AnyFile);
}

void QnTwoStepFileDialog::setAcceptMode(QFileDialog::AcceptMode mode) {
    Q_ASSERT(mode == QFileDialog::AcceptSave);
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
