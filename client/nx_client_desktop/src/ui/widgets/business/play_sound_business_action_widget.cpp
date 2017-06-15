#include <core/resource/camera_resource.h>
#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>

#include <business/business_action_parameters.h>
#include <ui/style/resource_icon_cache.h>

#include <nx/audio/audiodevice.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/media/audio_player.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <core/resource_management/resource_pool.h>

using namespace nx::client::desktop;

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PlaySoundBusinessActionWidget)
{
    ui->setupUi(this);

    ui->volumeSlider->setValue(qRound(nx::audio::AudioDevice::instance()->volume() * 100));

    QnNotificationSoundModel* soundModel = context()->instance<ServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setModel(soundModel);

    connect(soundModel, &QnNotificationSoundModel::listLoaded,
        this, &QnPlaySoundBusinessActionWidget::updateCurrentIndex);

    connect(ui->pathComboBox, QnComboboxCurrentIndexChanged,
        this, &QnPlaySoundBusinessActionWidget::paramsChanged);

    connect(ui->testButton, &QPushButton::clicked,
        this, &QnPlaySoundBusinessActionWidget::at_testButton_clicked);

    connect(ui->manageButton, &QPushButton::clicked,
        this, &QnPlaySoundBusinessActionWidget::at_manageButton_clicked);

    connect(ui->volumeSlider, &QSlider::valueChanged,
        this, &QnPlaySoundBusinessActionWidget::at_volumeSlider_valueChanged);

    connect(nx::audio::AudioDevice::instance(), &nx::audio::AudioDevice::volumeChanged, this,
        [this]
        {
            QScopedValueRollback<bool> updatingRollback(m_updating, true);
            ui->volumeSlider->setValue(qRound(nx::audio::AudioDevice::instance()->volume() * 100));
        });

    connect(ui->playToClient, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            ui->selectUsersButton->setVisible(checked);
            paramsChanged();
        });

    ui->playToClient->setFixedHeight(ui->selectUsersButton->minimumSizeHint().height());
    ui->selectUsersButton->setVisible(ui->playToClient->isChecked());
    setSubjectsButton(ui->selectUsersButton);

    setHelpTopic(this, Qn::EventsActions_PlaySound_Help);
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}

void QnPlaySoundBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before,                ui->playToClient);
    setTabOrder(ui->playToClient,      ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->pathComboBox);
    setTabOrder(ui->pathComboBox,      ui->manageButton);
    setTabOrder(ui->manageButton,      ui->volumeSlider);
    setTabOrder(ui->volumeSlider,      ui->testButton);
    setTabOrder(ui->testButton,         after);
}

void QnPlaySoundBusinessActionWidget::updateCurrentIndex()
{
    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    QnNotificationSoundModel* soundModel =
        context()->instance<ServerNotificationCache>()->persistentGuiModel();
    ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model())
        return;

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto params = model()->actionParams();
    if (fields & QnBusiness::ActionParamsField)
    {
        m_filename = params.url;
        QnNotificationSoundModel* soundModel =
            context()->instance<ServerNotificationCache>()->persistentGuiModel();
        ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
        ui->playToClient->setChecked(params.playToClient);
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QnNotificationSoundModel* soundModel =
        context()->instance<ServerNotificationCache>()->persistentGuiModel();
    if (!soundModel->loaded())
        return;

    auto params = model()->actionParams();
    params.url = soundModel->filenameByRow(ui->pathComboBox->currentIndex());
    params.playToClient = ui->playToClient->isChecked();
    model()->setActionParams(params);
}

void QnPlaySoundBusinessActionWidget::enableTestButton()
{
    ui->testButton->setEnabled(true);
}

void QnPlaySoundBusinessActionWidget::at_testButton_clicked()
{
    QnNotificationSoundModel* soundModel =
        context()->instance<ServerNotificationCache>()->persistentGuiModel();
    if (!soundModel->loaded())
        return;

    QString soundUrl = soundModel->filenameByRow(ui->pathComboBox->currentIndex());

    if (soundUrl.isEmpty())
        return;

    QString filePath = context()->instance<ServerNotificationCache>()->getFullPath(soundUrl);
    if (!QFileInfo(filePath).exists())
        return;

    if (AudioPlayer::playFileAsync(filePath, this, SLOT(enableTestButton())))
        ui->testButton->setEnabled(false);
}

void QnPlaySoundBusinessActionWidget::at_manageButton_clicked()
{
    QnNotificationSoundManagerDialog dialog(this);
    dialog.exec();
}

void QnPlaySoundBusinessActionWidget::at_soundModel_itemChanged(const QString& filename)
{
    if (m_filename != filename)
        return;
}

void QnPlaySoundBusinessActionWidget::at_volumeSlider_valueChanged(int value)
{
    if (m_updating)
        return;

    nx::audio::AudioDevice::instance()->setVolume((qreal)value * 0.01);
}
