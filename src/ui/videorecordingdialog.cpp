#include "videorecordingdialog.h"
#include "ui_videorecordingdialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>

VideoRecordingDialog::VideoRecordingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VideoRecordingDialog)
{
    ui->setupUi(this);
#ifndef Q_OS_WIN
    ui->fullscreenNoAeroButton->setVisible(false);
#elif
    ui->fullscreenNoAeroButton->setEnabled(true);
#endif
    connect(ui->saveAsButton, SIGNAL(pressed()), SLOT(onSaveAsPressed()));
    connect(ui->startRecordingButton, SIGNAL(pressed()), SLOT(onStartRecordingPressed()));
}

VideoRecordingDialog::~VideoRecordingDialog()
{
    delete ui;
}

QString VideoRecordingDialog::filePath() const
{
    return ui->fileLineEdit->text();
}

VideoRecordingDialog::CaptureMode VideoRecordingDialog::captureMode() const
{
    if (ui->fullscreenButton->isChecked())
        return FullScreenMode;
    else if (ui->fullscreenNoAeroButton->isChecked())
        return FullScreenNoeroMode;
    else
        return WindowMode;
}

VideoRecordingDialog::DecoderQuality VideoRecordingDialog::decoderQuality() const
{
    if (ui->bestButton->isChecked())
        return BestQuality;
    else if (ui->balancedButton->isChecked())
        return BalancedQuality;
    else
        return PerformanceQuality;
}

VideoRecordingDialog::Resolution VideoRecordingDialog::resolution() const
{
    return (Resolution)ui->resolutionComboBox->currentIndex();
}

void VideoRecordingDialog::onSaveAsPressed()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save As");
    ui->fileLineEdit->setText(fileName);
}

void VideoRecordingDialog::onStartRecordingPressed()
{
    QString path = ui->fileLineEdit->text();

    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Wrong file name"), tr("File name can't be empty"));
        return;
    }

    CaptureMode c = captureMode();
    DecoderQuality q = decoderQuality();
    Resolution r = resolution();

    accept();
}
