// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "volume_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/audio/audiodevice.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/rules/action_builder_fields/sound_field.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/actions/play_sound_action.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>
#include <utils/media/audio_player.h>

namespace nx::vms::client::desktop::rules {

namespace {

constexpr auto kOneHundredPercent = 100;

} // namespace

VolumePicker::VolumePicker(
    vms::rules::VolumeField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    PlainFieldPickerWidget<vms::rules::VolumeField>(field, context, parent)
{
    auto contentLayout = new QHBoxLayout;

    contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    m_volumeSlider = new QSlider;
    m_volumeSlider->setOrientation(Qt::Horizontal);
    m_volumeSlider->setMaximum(kOneHundredPercent);
    contentLayout->addWidget(m_volumeSlider);

    m_testPushButton = new QPushButton;
    m_testPushButton->setText(tr("Test"));
    contentLayout->addWidget(m_testPushButton);

    m_contentWidget->setLayout(contentLayout);

    connect(
        m_volumeSlider,
        &QSlider::valueChanged,
        this,
        &VolumePicker::onVolumeChanged);

    connect(
        m_testPushButton,
        &QPushButton::clicked,
        this,
        &VolumePicker::onTestButtonClicked);
}

void VolumePicker::updateUi()
{
    QSignalBlocker blocker{m_volumeSlider};
    m_volumeSlider->setValue(qRound(m_field->value() * kOneHundredPercent));
}

void VolumePicker::onVolumeChanged()
{
    const auto newValue = static_cast<float>(m_volumeSlider->value()) / kOneHundredPercent;
    m_field->setValue(newValue);

    setEdited();

    if (m_inProgress)
        nx::audio::AudioDevice::instance()->setVolume(newValue);
}

void VolumePicker::onTestButtonClicked()
{
    const auto actionId = getActionDescriptor()->id;
    if (actionId == vms::rules::utils::type<vms::rules::SpeakAction>())
    {
        auto textField =
            getActionField<vms::rules::TextWithFields>(vms::rules::utils::kTextFieldName);
        if (!NX_ASSERT(textField))
            return;

        const auto text = textField->text();
        if (text.isEmpty())
            return;

        m_audioDeviceCachedVolume = nx::audio::AudioDevice::instance()->volume();
        nx::audio::AudioDevice::instance()->setVolume(m_field->value());
        if (AudioPlayer::sayTextAsync(text, this, [this] { onTextSaid(); }))
            m_testPushButton->setEnabled(false);
    }
    else if (actionId == vms::rules::utils::type<vms::rules::RepeatSoundAction>()
        || actionId == vms::rules::utils::type<vms::rules::PlaySoundAction>())
    {
        auto soundField =
            getActionField<vms::rules::SoundField>(vms::rules::utils::kSoundFieldName);
        if (!NX_ASSERT(soundField))
            return;

        QString soundUrl = soundField->value();
        if (soundUrl.isEmpty())
            return;

        QString filePath = systemContext()->serverNotificationCache()->getFullPath(soundUrl);
        if (!QFileInfo(filePath).exists())
            return;

        m_audioDeviceCachedVolume = nx::audio::AudioDevice::instance()->volume();
        nx::audio::AudioDevice::instance()->setVolume(m_field->value());
        if (AudioPlayer::playFileAsync(filePath, this, [this] { onTextSaid(); }))
        {
            m_inProgress = true;
            m_testPushButton->setEnabled(false);
        }
    }
    else
    {
        NX_ASSERT(false, "Unexpected action type is used");
    }
}

void VolumePicker::onTextSaid()
{
    nx::audio::AudioDevice::instance()->setVolume(m_audioDeviceCachedVolume);
    m_inProgress = false;
    m_testPushButton->setEnabled(true);
}

} // namespace nx::vms::client::desktop::rules
