// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "say_text_business_action_widget.h"
#include "ui_say_text_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/audio/audiodevice.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <utils/media/audio_player.h>

using namespace nx::vms::client::desktop;

QnSayTextBusinessActionWidget::QnSayTextBusinessActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::SayTextBusinessActionWidget)
{
    ui->setupUi(this);

    ui->volumeSlider->setValue(qRound(nx::audio::AudioDevice::instance()->volume() * 100));

    connect(ui->textEdit, &QLineEdit::textChanged,
        this, &QnSayTextBusinessActionWidget::paramsChanged);

    connect(ui->testButton, &QPushButton::clicked,
        this, &QnSayTextBusinessActionWidget::at_testButton_clicked);

    connect(ui->volumeSlider, &QSlider::valueChanged,
        this, &QnSayTextBusinessActionWidget::at_volumeSlider_valueChanged);

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

    setHelpTopic(this, Qn::EventsActions_Speech_Help);
}

QnSayTextBusinessActionWidget::~QnSayTextBusinessActionWidget()
{
}

void QnSayTextBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before,                ui->playToClient);
    setTabOrder(ui->playToClient,      ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->textEdit);
    setTabOrder(ui->textEdit,          ui->volumeSlider);
    setTabOrder(ui->volumeSlider,      ui->testButton);
    setTabOrder(ui->testButton,        after);
}

void QnSayTextBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    const auto params = model()->actionParams();
    if (fields.testFlag(Field::actionParams))
    {
        ui->textEdit->setText(params.sayText);
        ui->playToClient->setChecked(params.playToClient);
    }
}

void QnSayTextBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto params = model()->actionParams();
    params.sayText = ui->textEdit->text();
    params.playToClient = ui->playToClient->isChecked();
    model()->setActionParams(params);
}

void QnSayTextBusinessActionWidget::enableTestButton()
{
    ui->testButton->setEnabled(true);
}

void QnSayTextBusinessActionWidget::at_testButton_clicked()
{
    if (ui->textEdit->text().isEmpty())
        return;

    if (AudioPlayer::sayTextAsync(ui->textEdit->text(), this, SLOT(enableTestButton())))
        ui->testButton->setEnabled(false);
}

void QnSayTextBusinessActionWidget::at_volumeSlider_valueChanged(int value)
{
    if (m_updating)
        return;

    nx::audio::AudioDevice::instance()->setVolume((qreal)value * 0.01);
}
