// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/audio/audiodevice.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <utils/media/audio_player.h>

using namespace nx::vms::client::desktop;

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(
    WindowContext* context,
    QWidget* parent)
    :
    base_type(context->system(), parent),
    ui(new Ui::PlaySoundBusinessActionWidget),
    m_serverNotificationCache(context->workbenchContext()->instance<ServerNotificationCache>())
{
    ui->setupUi(this);

    ui->volumeSlider->setValue(qRound(nx::audio::AudioDevice::instance()->volume() * 100));

    QnNotificationSoundModel* soundModel = m_serverNotificationCache->persistentGuiModel();
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

    setHelpTopic(this, HelpTopic::Id::EventsActions_PlaySound);
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

    QnNotificationSoundModel* soundModel = m_serverNotificationCache->persistentGuiModel();
    ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto params = model()->actionParams();
    if (fields.testFlag(Field::actionParams))
    {
        m_filename = params.url;
        QnNotificationSoundModel* soundModel = m_serverNotificationCache->persistentGuiModel();
        ui->pathComboBox->setCurrentIndex(soundModel->rowByFilename(m_filename));
        ui->playToClient->setChecked(params.playToClient);
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QnNotificationSoundModel* soundModel = m_serverNotificationCache->persistentGuiModel();
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
    QnNotificationSoundModel* soundModel = m_serverNotificationCache->persistentGuiModel();
    if (!soundModel->loaded())
        return;

    QString soundUrl = soundModel->filenameByRow(ui->pathComboBox->currentIndex());

    if (soundUrl.isEmpty())
        return;

    QString filePath = m_serverNotificationCache->getFullPath(soundUrl);
    if (!QFileInfo(filePath).exists())
        return;

    if (AudioPlayer::playFileAsync(filePath, this, [this]{ enableTestButton(); }))
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
