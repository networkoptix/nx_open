#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnNotificationSoundManagerDialog),
    m_cache(new QnAppServerNotificationCache(this)),
    m_loadingCounter(0),
    m_total(0)
{
    ui->setupUi(this);
    ui->removeButton->setVisible(false);

    connect(m_cache, SIGNAL(fileListReceived(QStringList,bool)), this, SLOT(at_fileListReceived(QStringList,bool)));
    connect(m_cache, SIGNAL(fileDownloaded(QString,bool)), this, SLOT(at_fileDownloaded(QString,bool)), Qt::QueuedConnection);

    m_cache->getFileList();
    updateLoadingStatus();
}

QnNotificationSoundManagerDialog::~QnNotificationSoundManagerDialog()
{
}


void QnNotificationSoundManagerDialog::updateLoadingStatus() {
    int pageIndex = (m_loadingCounter == 0) ? 0 : 1;
    ui->stackedWidget->setCurrentIndex(pageIndex);

    if (m_loadingCounter > 0) {
        ui->progressBar->setValue( (m_total - m_loadingCounter)*100 / m_total);
    }
}

void QnNotificationSoundManagerDialog::at_fileListReceived(const QStringList &filenames, bool ok) {
    if (!ok)
        //TODO: #GDM show warning message
        return;

    m_total = filenames.size();

    foreach(QString filename, filenames) {
        m_cache->downloadFile(filename);
    }

    m_loadingCounter = m_total;
    updateLoadingStatus();
}

void QnNotificationSoundManagerDialog::at_fileDownloaded(const QString &filename, bool ok) {
    m_loadingCounter--;
    updateLoadingStatus();

    if (!ok)
        //TODO: #GDM show warning message
        return;

    //TODO: #GDM extractMetadata
    QString title = filename;
    ui->listWidget->addItem(new QListWidgetItem(title));


}
