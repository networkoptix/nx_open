#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <QtCore/QFileInfo>

#include <business/business_action_parameters.h>

#include <openal/qtvaudiodevice.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/media/audio_player.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PlaySoundBusinessActionWidget)
{
    ui->setupUi(this);

    ui->volumeSlider->setValue(qRound(QtvAudioDevice::instance()->volume() * 100));

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setModel(soundModel);

    connect(soundModel, SIGNAL(listLoaded()), this, SLOT(updateCurrentIndex()));

    connect(ui->pathComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(at_testButton_clicked()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));
    connect(ui->volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(at_volumeSlider_valueChanged(int)));

    setHelpTopic(this, Qn::EventsActions_PlaySound_Help);
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}

void QnPlaySoundBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before,                 ui->pathComboBox);
    setTabOrder(ui->pathComboBox,       ui->manageButton);
    setTabOrder(ui->manageButton,       ui->volumeSlider);
    setTabOrder(ui->volumeSlider,       ui->testButton);
    setTabOrder(ui->testButton, after);
}

void QnPlaySoundBusinessActionWidget::updateCurrentIndex() {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::ActionParamsField) {
        m_filename = model->actionParams().soundUrl;
        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
        ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    if (!soundModel->loaded())
        return;

    QnBusinessActionParameters params;
    params.soundUrl = soundModel->filenameByRow(ui->pathComboBox->currentIndex());
    model()->setActionParams(params);
}

void QnPlaySoundBusinessActionWidget::enableTestButton() {
    ui->testButton->setEnabled(true);
}

void QnPlaySoundBusinessActionWidget::at_testButton_clicked() {
    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    if (!soundModel->loaded())
        return;

    QString soundUrl = soundModel->filenameByRow(ui->pathComboBox->currentIndex());

    if (soundUrl.isEmpty())
        return;

    QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(soundUrl);
    if (!QFileInfo(filePath).exists())
        return;

    if (AudioPlayer::playFileAsync(filePath, this, SLOT(enableTestButton())))
        ui->testButton->setEnabled(false);

}

void QnPlaySoundBusinessActionWidget::at_manageButton_clicked() {
    QScopedPointer<QnNotificationSoundManagerDialog> dialog(new QnNotificationSoundManagerDialog(this));
    dialog->exec();
}

void QnPlaySoundBusinessActionWidget::at_soundModel_itemChanged(const QString &filename) {
    if (m_filename != filename)
        return;
}

void QnPlaySoundBusinessActionWidget::at_volumeSlider_valueChanged(int value) {
    QtvAudioDevice::instance()->setVolume((qreal)value * 0.01);
}
