#ifndef YOUTUBEUPLOADPROGRESSDIALOG_P_H
#define YOUTUBEUPLOADPROGRESSDIALOG_P_H

#include <QtGui/QProgressDialog>

class YouTubeUploadProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit YouTubeUploadProgressDialog(QWidget *parent = 0);
    ~YouTubeUploadProgressDialog();

public Q_SLOTS:
    void reject();

protected:
    void closeEvent(QCloseEvent *e);

private Q_SLOTS:
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void uploadFailed(const QString &message, int code);
    void uploadFinished();
};

#endif // YOUTUBEUPLOADPROGRESSDIALOG_P_H
