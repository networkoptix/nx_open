#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <QtCore/QFileInfo>

#include <business/business_action_parameters.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/media/audio_player.h>


QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPlaySoundBusinessActionWidget),
    m_cache(new QnAppServerNotificationCache(this)),
    m_loaded(false)
{
    ui->setupUi(this);

    connect(ui->pathComboBox, SIGNAL(editTextChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(at_playButton_clicked()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));

    connect(m_cache, SIGNAL(fileListReceived(QStringList,bool)), this, SLOT(at_fileListReceived(QStringList,bool)));
    m_cache->getFileList();
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}

void QnPlaySoundBusinessActionWidget::updateSoundUrl() {
    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    int idx = ui->pathComboBox->findData(m_soundUrl);
    if (idx > 0)
        ui->pathComboBox->setCurrentIndex(idx);
    else
        ui->pathComboBox->setCurrentIndex(0);
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    if (fields & QnBusiness::ActionParamsField) {
        m_soundUrl = QnBusinessActionParameters::getSoundUrl(model->actionParams());
        if (!m_loaded) {
            QString title = AudioPlayer::getTagValue(m_cache->getFullPath(m_soundUrl), m_cache->titleTag() );
            if (title.isEmpty())
                title = m_soundUrl;

            ui->pathComboBox->clear();
            ui->pathComboBox->addItem(title, m_soundUrl);
        }
        updateSoundUrl();
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating || !m_loaded)
        return;

    QnBusinessParams params;
    QnBusinessActionParameters::setSoundUrl(&params, ui->pathComboBox->itemData(ui->pathComboBox->currentIndex()).toString());
    model()->setActionParams(params);
}

void QnPlaySoundBusinessActionWidget::at_playButton_clicked() {
    if (m_soundUrl.isEmpty())
        return;

    QString filePath = m_cache->getFullPath(m_soundUrl);
    if (!QFileInfo(filePath).exists())
        return;

    AudioPlayer::playFileAsync(filePath);
}

void QnPlaySoundBusinessActionWidget::at_manageButton_clicked() {
    QScopedPointer<QnNotificationSoundManagerDialog> dialog(new QnNotificationSoundManagerDialog());
    if (dialog->exec() == QDialog::Rejected)
        return;
    m_loaded = false;
    ui->pathComboBox->setEnabled(false);
    m_cache->getFileList();
}

void QnPlaySoundBusinessActionWidget::at_fileListReceived(const QStringList &filenames, bool ok) {
    if (!ok)
        return;

    ui->pathComboBox->clear();
    foreach(const QString &filename, filenames) {
        QString title = AudioPlayer::getTagValue(m_cache->getFullPath(filename), m_cache->titleTag() );
        if (title.isEmpty())
            title = filename;
        ui->pathComboBox->addItem(title, filename); //TODO: #GDM metatada, filename in userData
    }
    m_loaded = true;
    ui->pathComboBox->setEnabled(true);
    updateSoundUrl();
}
