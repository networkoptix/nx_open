#include <core/resource/camera_resource.h>
#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <QtCore/QFileInfo>

#include <business/business_action_parameters.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>

#include <openal/qtvaudiodevice.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/media/audio_player.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <core/resource_management/resource_pool.h>

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PlaySoundBusinessActionWidget)
{
    ui->setupUi(this);

    ui->volumeSlider->setValue(qRound(QtvAudioDevice::instance()->volume() * 100));
    ui->actionTargetsHolder->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setModel(soundModel);

    connect(soundModel, SIGNAL(listLoaded()), this, SLOT(updateCurrentIndex()));

    connect(ui->pathComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(at_testButton_clicked()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));
    connect(ui->volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(at_volumeSlider_valueChanged(int)));
    connect(ui->actionTargetsHolder, SIGNAL(clicked()), this, SLOT(at_actionTargetsHolder_clicked()));
    connect(ui->playToClient, SIGNAL(stateChanged(int)), this, SLOT(paramsChanged()));

    connect(QtvAudioDevice::instance(), &QtvAudioDevice::volumeChanged, this, [this] {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->volumeSlider->setValue(qRound(QtvAudioDevice::instance()->volume() * 100));
    });

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

QString QnPlaySoundBusinessActionWidget::getActionTargetsHolderText(
    const QnBusinessActionParameters& params) const
{
    auto resources = qnResPool->getResources<QnResource>(params.additionalResources);

    if(resources.empty())
        return tr("<Choose camera>");

    auto errorText = QnCameraAudioTransmitPolicy::getText(resources);
    if (!errorText.isEmpty())
        return errorText;

    if (resources.size() == 1)
        return resources[0]->getName();
    else
        return tr("%1 Camera(s)").arg(resources.size());
}

void QnPlaySoundBusinessActionWidget::updateCurrentIndex() {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusiness::Fields fields) {
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    auto params = model()->actionParams();
    if (fields & QnBusiness::ActionParamsField) {
        m_filename = params.url;
        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
        ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
        ui->playToClient->setChecked(params.playToClient);
        ui->actionTargetsHolder->setText(getActionTargetsHolderText(params));
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    if (!soundModel->loaded())
        return;

    auto params = model()->actionParams();
    params.url = soundModel->filenameByRow(ui->pathComboBox->currentIndex());
    params.playToClient = ui->playToClient->isChecked();
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
    if (m_updating)
        return;

    QtvAudioDevice::instance()->setVolume((qreal)value * 0.01);
}

void QnPlaySoundBusinessActionWidget::at_actionTargetsHolder_clicked() {

    auto params = model()->actionParams();
    QnResourceSelectionDialog dialog(this);
    QnResourceList selected = qnResPool->getResources(params.additionalResources);

    dialog.setDelegate(
        new QnCheckResourceAndWarnDelegate<QnCameraAudioTransmitPolicy>(this));
    dialog.setSelectedResources(selected);

    if (dialog.exec() != QDialog::Accepted)
        return;

    params.additionalResources.clear();
    for(const auto& res: dialog.selectedResources())
        params.additionalResources.push_back(res->getId());

    model()->setActionParams(params);
}
