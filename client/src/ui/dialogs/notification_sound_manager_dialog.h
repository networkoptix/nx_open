#ifndef NOTIFICATION_SOUND_MANAGER_DIALOG_H
#define NOTIFICATION_SOUND_MANAGER_DIALOG_H

#include <QDialog>

#include <utils/app_server_notification_cache.h>

namespace Ui {
    class QnNotificationSoundManagerDialog;
}

class QnNotificationSoundManagerDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit QnNotificationSoundManagerDialog(QWidget *parent = 0);
    ~QnNotificationSoundManagerDialog();
private slots:
    void at_fileListReceived(const QStringList &filenames, bool ok);
    void at_fileDownloaded(const QString &filename, bool ok);
    void at_fileUploaded(const QString &filename, bool ok);

    void at_addButton_clicked();
private:
    void updateLoadingStatus();

    QScopedPointer<Ui::QnNotificationSoundManagerDialog> ui;

    QnAppServerNotificationCache *m_cache;
    int m_loadingCounter;
    int m_total;
};

#endif // NOTIFICATION_SOUND_MANAGER_DIALOG_H
