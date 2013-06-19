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

namespace {
    QString speechPrefix = QLatin1String("speech://");
}


QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPlaySoundBusinessActionWidget)
{
    ui->setupUi(this);



    ui->volumeSlider->setValue(qRound(QtvAudioDevice::instance()->volume() * 100));

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setModel(soundModel);

    connect(soundModel, SIGNAL(listLoaded()), this, SLOT(updateCurrentIndex()));
    connect(ui->soundRadioButton, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));  // one radiobutton toggle signal is enough

    connect(ui->soundRadioButton, SIGNAL(toggled(bool)), ui->pathComboBox, SLOT(setEnabled(bool)));
    connect(ui->soundRadioButton, SIGNAL(toggled(bool)), ui->manageButton, SLOT(setEnabled(bool)));
    connect(ui->soundRadioButton, SIGNAL(toggled(bool)), ui->speechLineEdit, SLOT(setDisabled(bool)));

    connect(ui->speechRadioButton, SIGNAL(toggled(bool)), ui->speechLineEdit, SLOT(setEnabled(bool)));

    connect(ui->pathComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->speechLineEdit, SIGNAL(textChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(at_testButton_clicked()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));
    connect(ui->volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(at_volumeSlider_valueChanged(int)));

}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}

void QnPlaySoundBusinessActionWidget::updateCurrentIndex() {
    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField) {
        QString soundUrl = model->actionParams().getSoundUrl();

        bool speech = soundUrl.startsWith(speechPrefix);
        ui->speechRadioButton->setChecked(speech);
        ui->soundRadioButton->setChecked(!speech);
        if (speech) {
            ui->speechLineEdit->setText(soundUrl.mid(speechPrefix.size()));
        } else {
            m_filename = soundUrl;

            QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
            ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
        }
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QString soundUrl;
    if (ui->soundRadioButton->isChecked()) {
        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
        if (soundModel->loaded())
            return;

        soundUrl = soundModel->filenameByRow(ui->pathComboBox->currentIndex());
    } else {
        soundUrl = speechPrefix + ui->speechLineEdit->text();
    }

    QnBusinessActionParameters params;
    params.setSoundUrl(soundUrl);
    model()->setActionParams(params);
}

void QnPlaySoundBusinessActionWidget::enableTestButton() {
    ui->testButton->setEnabled(true);
}

void QnPlaySoundBusinessActionWidget::at_testButton_clicked() {
    if (ui->soundRadioButton->isChecked()) {
        if (m_filename.isEmpty())
            return;

        QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(m_filename);
        if (!QFileInfo(filePath).exists())
            return;

        if (AudioPlayer::playFileAsync(filePath, this, SLOT(enableTestButton())))
            ui->testButton->setEnabled(false);
    } else {
        if (ui->speechLineEdit->text().isEmpty())
            return;
        if (AudioPlayer::sayTextAsync(ui->speechLineEdit->text(), this, SLOT(enableTestButton())))
            ui->testButton->setEnabled(false);
    }
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
