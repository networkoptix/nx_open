#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <QtCore/QFileInfo>

#include <business/business_action_parameters.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/media/audio_player.h>


QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPlaySoundBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->pathComboBox, SIGNAL(editTextChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(at_playButton_clicked()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setModel(soundModel);

    //TODO: #GDM update only required
    connect(soundModel, SIGNAL(itemChanged(QString)), this, SLOT(at_soundModel_itemChanged(QString)));
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}

void QnPlaySoundBusinessActionWidget::updateSoundUrl() {
    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    if (fields & QnBusiness::ActionParamsField) {
        m_filename = model->actionParams().getSoundUrl();
        updateSoundUrl();
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    QString filename = soundModel->filenameByRow(ui->pathComboBox->currentIndex());
    QnBusinessActionParameters params;
    params.setSoundUrl(filename);
    model()->setActionParams(params);
}

void QnPlaySoundBusinessActionWidget::at_playButton_clicked() {
    if (m_filename.isEmpty())
        return;

    QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(m_filename);
    if (!QFileInfo(filePath).exists())
        return;

    AudioPlayer::playFileAsync(filePath);
}

void QnPlaySoundBusinessActionWidget::at_manageButton_clicked() {
    QScopedPointer<QnNotificationSoundManagerDialog> dialog(new QnNotificationSoundManagerDialog());
    dialog->exec();
}

void QnPlaySoundBusinessActionWidget::at_soundModel_itemChanged(const QString &filename) {
    if (m_filename != filename)
        return;


}
