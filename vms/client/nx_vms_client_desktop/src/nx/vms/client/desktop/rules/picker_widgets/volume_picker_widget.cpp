// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "volume_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/audio/audiodevice.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/utils/field.h>
#include <utils/media/audio_player.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

namespace {

constexpr auto kOneHundredPercent = 100;

} // namespace

VolumePickerWidget::VolumePickerWidget(SystemContext* context, QWidget* parent):
    FieldPickerWidget<vms::rules::VolumeField>(context, parent)
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
        &VolumePickerWidget::onVolumeChanged);

    connect(
        m_testPushButton,
        &QPushButton::clicked,
        this,
        &VolumePickerWidget::onTestButtonClicked);
}

void VolumePickerWidget::onFieldsSet()
{
    QSignalBlocker blocker{m_volumeSlider};
    m_volumeSlider->setValue(qRound(m_field->value() * kOneHundredPercent));
}

void VolumePickerWidget::onVolumeChanged()
{
    m_field->setValue(static_cast<float>(m_volumeSlider->value()) / kOneHundredPercent);
}

void VolumePickerWidget::onTestButtonClicked()
{
    auto textField = getLinkedField<vms::rules::TextWithFields>(vms::rules::utils::kTextFieldName);
    if (!NX_ASSERT(textField))
        return;

    const auto text = textField->text();
    if (text.isEmpty())
        return;

    m_audioDeviceCachedVolume = nx::audio::AudioDevice::instance()->volume();
    nx::audio::AudioDevice::instance()->setVolume(m_field->value());
    if (AudioPlayer::sayTextAsync(text, this, SLOT(onTextSaid())))
        m_testPushButton->setEnabled(false);
}

void VolumePickerWidget::onTextSaid()
{
    nx::audio::AudioDevice::instance()->setVolume(m_audioDeviceCachedVolume);
    m_testPushButton->setEnabled(true);
}

} // namespace nx::vms::client::desktop::rules
