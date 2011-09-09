#ifndef YOUTUBEUPLOADDIALOG_H
#define YOUTUBEUPLOADDIALOG_H

#include <QtGui/QDialog>

class CLDevice;
class YouTubeUploader;

namespace Ui {
    class YouTubeUploadDialog;
}

class YouTubeUploadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YouTubeUploadDialog(CLDevice *dev = 0, QWidget *parent = 0);
    ~YouTubeUploadDialog();

public Q_SLOTS:
    void accept();

protected:
    void retranslateUi();

    void changeEvent(QEvent *e);
    void keyPressEvent(QKeyEvent *e);

private Q_SLOTS:
    void browseFile();
    void categoryListLoaded();
    void authFailed();
    void authFinished();

private:
    Ui::YouTubeUploadDialog *ui;

    YouTubeUploader *m_youtubeuploader;
};

#endif // YOUTUBEUPLOADDIALOG_H
