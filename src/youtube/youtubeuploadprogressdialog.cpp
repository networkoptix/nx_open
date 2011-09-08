#include "youtubeuploadprogressdialog_p.h"

#include <QtGui/QMessageBox>

YouTubeUploadProgressDialog::YouTubeUploadProgressDialog(QWidget *parent) :
    QProgressDialog(parent)
{
    setAutoClose(false);
    setAutoReset(false);

    connect(this, SIGNAL(canceled()), this, SLOT(reject()));
}

YouTubeUploadProgressDialog::~YouTubeUploadProgressDialog()
{
}

void YouTubeUploadProgressDialog::reject()
{
    if (close())
        QProgressDialog::reject();
}

void YouTubeUploadProgressDialog::closeEvent(QCloseEvent *e)
{
    e->setAccepted(QMessageBox::question(this, tr("Abort uploading?"),
                                         tr("Are you sure you want abort uploading and close dialog?"),
                                         QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes);
    if (!e->isAccepted())
        return;

    QProgressDialog::closeEvent(e);
}

void YouTubeUploadProgressDialog::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (maximum() != bytesTotal)
        setMaximum(bytesTotal);
    setValue(bytesSent);
}

void YouTubeUploadProgressDialog::uploadFailed(const QString &message, int code)
{
    if (QMessageBox::warning(this, tr("Upload failed"), tr("Upload failed due to error: %1 (%2)").arg(message).arg(code)) == QMessageBox::Ok)
        close();
}

void YouTubeUploadProgressDialog::uploadFinished()
{
    accept();
}
