#ifndef YOUTUBEUPLOADDIALOG_H
#define YOUTUBEUPLOADDIALOG_H

#include <QtGui/QDialog>
#include "core/resource/resource.h"


class QnResource;
class YouTubeUploader;

namespace Ui {
    class YouTubeUploadDialog;
}

class YouTubeUploadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YouTubeUploadDialog(QnResourcePtr dev = QnResourcePtr(), QWidget *parent = 0);
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
