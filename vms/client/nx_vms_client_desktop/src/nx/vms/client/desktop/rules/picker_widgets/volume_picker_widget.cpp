// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "volume_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/audio/audiodevice.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/rules/action_builder_fields/sound_field.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/workbench/workbench_context.h>
#include <utils/media/audio_player.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

namespace {

constexpr auto kOneHundredPercent = 100;

} // namespace

VolumePicker::VolumePicker(WindowContext* context, ParamsWidget* parent):
    PlainFieldPickerWidget<vms::rules::VolumeField>(context->system(), parent),
    WindowContextAware(context)
{
    auto contentLayout = new QHBoxLayout;

    contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    m_volumeSlider = new QSlider;
    m_volumeSlider->setOrientation(Qt::Horizontal);
    m_volumeSlider->setMaximum(kOneHundredPercent);
    contentLayout->addWidget(m_volumeSlider);

    m_testPushButton = new QPushButton;
    m_testPushButton->setText(CommonPickerWidgetStrings::testButtonDisplayText());
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
    m_volumeSlider->setValue(qRound(theField()->value() * kOneHundredPercent));
}

void VolumePicker::onVolumeChanged()
{
    const auto newValue = static_cast<float>(m_volumeSlider->value()) / kOneHundredPercent;
    theField()->setValue(newValue);
    if (m_inProgress)
        nx::audio::AudioDevice::instance()->setVolume(newValue);
}

void VolumePicker::onTestButtonClicked()
{
    const auto linkedFields = descriptor()->linkedFields;
    if (!NX_ASSERT(!linkedFields.empty(), "Linked field is not declared"))
        return;

    if (linkedFields.contains(vms::rules::utils::kTextFieldName))
    {
        auto textField =
            getActionField<vms::rules::TextWithFields>(vms::rules::utils::kTextFieldName);
        if (!NX_ASSERT(textField))
            return;

        const auto text = textField->text();
        if (text.isEmpty())
            return;

        m_audioDeviceCachedVolume = nx::audio::AudioDevice::instance()->volume();
        nx::audio::AudioDevice::instance()->setVolume(theField()->value());
        if (AudioPlayer::sayTextAsync(text, this, [this] { onTextSaid(); }))
            m_testPushButton->setEnabled(false);
    }
    else if (linkedFields.contains(vms::rules::utils::kSoundFieldName))
    {
        auto soundField =
            getActionField<vms::rules::SoundField>(vms::rules::utils::kSoundFieldName);
        if (!NX_ASSERT(soundField))
            return;

        QString soundUrl = soundField->value();
        if (soundUrl.isEmpty())
            return;

        QString filePath = workbenchContext()->instance<ServerNotificationCache>()->getFullPath(soundUrl);
        if (!QFileInfo(filePath).exists())
            return;

        m_audioDeviceCachedVolume = nx::audio::AudioDevice::instance()->volume();
        nx::audio::AudioDevice::instance()->setVolume(theField()->value());
        if (AudioPlayer::playFileAsync(filePath, this, [this] { onTextSaid(); }))
        {
            m_inProgress = true;
            m_testPushButton->setEnabled(false);
        }
    }
    else
    {
        NX_ASSERT(false, "Unsupported linked field type declared");
    }
}

void VolumePicker::onTextSaid()
{
    nx::audio::AudioDevice::instance()->setVolume(m_audioDeviceCachedVolume);
    m_inProgress = false;
    m_testPushButton->setEnabled(true);
}

} // namespace nx::vms::client::desktop::rules
