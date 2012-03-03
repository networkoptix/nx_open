#ifndef YOUTUBEUPLOADDIALOG_H
#define YOUTUBEUPLOADDIALOG_H

#include <QtGui/QDialog>

#include "core/resource/resource.h"

class YouTubeUploader;

class QnWorkbenchContext;

namespace Ui {
    class YouTubeUploadDialog;
}

class YouTubeUploadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YouTubeUploadDialog(QnWorkbenchContext *context, QnResourcePtr resource = QnResourcePtr(0), QWidget *parent = 0);
    virtual ~YouTubeUploadDialog();

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

    QWeakPointer<QnWorkbenchContext> m_context;

    YouTubeUploader *m_youtubeuploader;
};

#endif // YOUTUBEUPLOADDIALOG_H
