// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_redirect_picker_widget.h"
#include "ui_audio_redirect_picker_widget.h"

#include <QtWidgets/QMenu>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/common/palette.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"
#include "nx/vms/client/desktop/style/helper.h"

namespace nx::vms::client::desktop {

AudioRedirectPickerWidget::AudioRedirectPickerWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    ui(new Ui::AudioRedirectPickerWidget)
{
    ui->setupUi(this);

    ui->redirectOptionDropdownWidget->setVisible(false);
    ui->redirectOptionLabel->setVisible(false);
    ui->devicePickerButton->setVisible(false);
    ui->errorLabel->setVisible(false);

    m_inactiveRedirectDropdownMenu = std::make_unique<QMenu>();
    m_inactiveRedirectDropdownMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    m_activeRedirectDropdownMenu = std::make_unique<QMenu>();
    m_activeRedirectDropdownMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    connect(ui->devicePickerButton, &QPushButton::clicked,
        [this]() { selectAudioRedirectDevice(); });

    setWarningStyle(ui->errorLabel);
}

AudioRedirectPickerWidget::~AudioRedirectPickerWidget()
{
}

void AudioRedirectPickerWidget::setup(StreamDirection streamDirection)
{
    if (!NX_ASSERT(!m_streamDirection, "An attempt to double initialize audio redirect picker"))
        return;

    m_streamDirection = streamDirection;

    ui->redirectOptionDropdownButton->setMenu(m_inactiveRedirectDropdownMenu.get());
    ui->redirectOptionDropdownButton->setText(inactiveRedirectLabelText());

    m_inactiveRedirectDropdownAction = m_inactiveRedirectDropdownMenu->addAction(
        inactiveRedirectMenuItemText(),
        [this]()
        {
            ui->redirectOptionDropdownButton->setText(activeRedirectLabelText());
            ui->redirectOptionDropdownButton->setMenu(m_activeRedirectDropdownMenu.get());
            ui->devicePickerButton->setVisible(true);
        });

    m_activeRedirectDropdownAction = m_activeRedirectDropdownMenu->addAction(
        activeRedirectMenuItemText(),
        [this]()
        {
            ui->redirectOptionDropdownButton->setText(inactiveRedirectLabelText());
            ui->redirectOptionDropdownButton->setMenu(m_inactiveRedirectDropdownMenu.get());
            ui->devicePickerButton->setVisible(false);
            if (!m_audioRedirectDeviceId.isNull())
            {
                m_audioRedirectDeviceId = QnUuid();
                emit audioRedirectDeviceIdChanged(m_audioRedirectDeviceId);
            }
        });
}

void AudioRedirectPickerWidget::loadState(const CameraSettingsDialogState& state)
{
    if (getRedirectDeviceId(state).isNull())
    {
        ui->redirectOptionDropdownButton->setMenu(m_inactiveRedirectDropdownMenu.get());
        ui->redirectOptionDropdownButton->setText(inactiveRedirectLabelText());
    }
    else
    {
        ui->redirectOptionDropdownButton->setMenu(m_activeRedirectDropdownMenu.get());
        ui->redirectOptionDropdownButton->setText(activeRedirectLabelText());
    }

    m_deviceId = state.singleCameraId();
    m_audioRedirectDeviceId = getRedirectDeviceId(state);

    const auto resourcePool = qnClientCoreModule->resourcePool();

    const auto device = resourcePool->getResourceById<QnVirtualCameraResource>(m_deviceId);
    const auto redirectDevice =
        resourcePool->getResourceById<QnVirtualCameraResource>(m_audioRedirectDeviceId);

    ui->redirectOptionDropdownWidget->setVisible(capabilityCheck(device));
    ui->redirectOptionLabel->setVisible(!capabilityCheck(device));

    ui->devicePickerButton->setVisible(!capabilityCheck(device) || !redirectDevice.isNull());
    if (!redirectDevice.isNull())
    {
        ui->redirectOptionLabel->setText(activeRedirectLabelText());
        ui->devicePickerButton->setText(redirectDevice->getName());
        ui->devicePickerButton->setIcon(
            qnResIconCache->icon(redirectDevice).pixmap(nx::style::Metrics::kDefaultIconSize));
        setWarningStyleOn(ui->devicePickerButton,
            !capabilityCheck(redirectDevice) || !sameServerCheck(redirectDevice));
        if (!capabilityCheck(redirectDevice))
            ui->errorLabel->setText(invalidRedirectErrorText());
        else if (!sameServerCheck(redirectDevice))
            ui->errorLabel->setText(differentServersErrorText());

        ui->errorLabel->setVisible(
            !capabilityCheck(redirectDevice) || !sameServerCheck(redirectDevice));
    }
    else
    {
        ui->redirectOptionLabel->setText(requiredRedirectLabelText());
        ui->devicePickerButton->setText(tr("Select Device..."));
        ui->devicePickerButton->setIcon(qnResIconCache->icon(QnResourceIconCache::Cameras)
            .pixmap(nx::style::Metrics::kDefaultIconSize));
        setWarningStyleOn(ui->devicePickerButton, false);
        ui->errorLabel->setVisible(false);
    }
    setPaletteColor(ui->devicePickerButton,
        QPalette::ButtonText,
        core::ColorTheme::instance()->color("light10"));
}

void AudioRedirectPickerWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    if (!m_streamDirection)
        return;

    m_inactiveRedirectDropdownAction->setText(inactiveRedirectMenuItemText());
    m_activeRedirectDropdownAction->setText(activeRedirectMenuItemText());
}

QnResourcePool* AudioRedirectPickerWidget::resourcePool() const
{
    return qnClientCoreModule->resourcePool();
}

void AudioRedirectPickerWidget::selectAudioRedirectDevice()
{
    const auto resourceFilter =
        [this](const QnResourcePtr& resource)
        {
            return (capabilityCheck(resource) && resource->getId() != m_deviceId)
                || (resource->getId() == m_audioRedirectDeviceId);
        };

    const auto resourceValidator =
        [this](const QnResourcePtr& redirectDevice)
        {
            return capabilityCheck(redirectDevice) && sameServerCheck(redirectDevice);
        };

    const auto alertTextProvider =
        [this](const QSet<QnResourcePtr>& resources)
        {
            if (resources.empty())
                return QString();

            const auto redirectDevice = *resources.begin();
            if (!sameServerCheck(redirectDevice))
            {
                return tr("%1 is connected to another server. Audio stream is not available")
                    .arg(redirectDevice->getName());
            }

            return QString();
        };

    auto cameraSelectionDialog = std::make_unique<CameraSelectionDialog>(
        resourceFilter,
        resourceValidator,
        alertTextProvider,
        this);
    cameraSelectionDialog->setAllCamerasSwitchVisible(*m_streamDirection == Input);
    cameraSelectionDialog->setAllowInvalidSelection(false);

    const auto resourceSelectionWidget = cameraSelectionDialog->resourceSelectionWidget();
    resourceSelectionWidget->setSelectionMode(ResourceSelectionMode::ExclusiveSelection);
    resourceSelectionWidget->setSelectedResourceId(m_audioRedirectDeviceId);
    resourceSelectionWidget->setShowRecordingIndicator(true);

    if (cameraSelectionDialog->exec() != QDialog::Accepted)
        return;

    if (m_audioRedirectDeviceId == resourceSelectionWidget->selectedResourceId())
        return;

    m_audioRedirectDeviceId = resourceSelectionWidget->selectedResourceId();
    emit audioRedirectDeviceIdChanged(m_audioRedirectDeviceId);
}

bool AudioRedirectPickerWidget::capabilityCheck(const QnResourcePtr& resource) const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return false;

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera.isNull())
        return false;

    if (*m_streamDirection == Input)
        return camera->isAudioSupported();
    else
        return camera->hasTwoWayAudio();
}

bool AudioRedirectPickerWidget::sameServerCheck(const QnResourcePtr& redirectDevice) const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return false;

    const auto device = resourcePool()->getResourceById(m_deviceId);
    if (device.isNull() || redirectDevice.isNull())
        return false;

    if (*m_streamDirection == Output)
        return true;

    return !device->getParentId().isNull()
        && device->getParentId() == redirectDevice->getParentId();
}

QnUuid AudioRedirectPickerWidget::getRedirectDeviceId(const CameraSettingsDialogState& state) const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QnUuid();

    if (!state.isSingleCamera())
        return QnUuid();

    return *m_streamDirection == Input
        ? state.singleCameraSettings.audioInputDeviceId
        : state.singleCameraSettings.audioOutputDeviceId;
}

QString AudioRedirectPickerWidget::activeRedirectLabelText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    return *m_streamDirection == Input
        ? tr("Use audio stream from")
        : tr("Transmit audio stream to");
}

QString AudioRedirectPickerWidget::inactiveRedirectLabelText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    const auto getInputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Use audio stream from this device"),
                tr("Use audio stream from this camera"));
        };

    const auto getOutputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Use this device for audio output"),
                tr("Use this camera for audio output"));
        };

    return *m_streamDirection == Input ? getInputString() : getOutputString();
}

QString AudioRedirectPickerWidget::requiredRedirectLabelText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    const auto getInputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("This device does not have an audio input, or it is not configured correctly."
                    " Select another device as an audio source."),
                tr("This camera does not have an audio input, or it is not configured correctly."
                    " Select another camera as an audio source."));
        };

    const auto getOutputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("This device does not have an audio output, or it is not configured correctly."
                    " Select another device for audio playback."),
                tr("This camera does not have an audio output, or it is not configured correctly."
                    " Select another camera for audio playback."));
        };

    return *m_streamDirection == Input ? getInputString() : getOutputString();
}

QString AudioRedirectPickerWidget::activeRedirectMenuItemText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    const auto getInputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Use audio stream from this device"),
                tr("Use audio stream from this camera"));
        };

    const auto getOutputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Use this device for audio output"),
                tr("Use this camera for audio output"));
        };

    return *m_streamDirection == Input ? getInputString() : getOutputString();
}

QString AudioRedirectPickerWidget::inactiveRedirectMenuItemText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    const auto getInputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Use audio stream from another device"),
                tr("Use audio stream from another camera"));
        };

    const auto getOutputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Transmit audio stream to another device"),
                tr("Transmit audio stream to another camera"));
        };

    return *m_streamDirection == Input ? getInputString() : getOutputString();
}

QString AudioRedirectPickerWidget::invalidRedirectErrorText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    return *m_streamDirection == Input
        ? tr("Selected device doesn't have audio input or it's not configured. "
            "Audio stream is not available.")
        : tr("Selected device doesn't have audio output or it's not configured. "
            "Audio stream is not available.");

    const auto getInputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("The selected device doesn't have an audio input, or it is not configured"
                    " correctly. The audio stream is not available."),
                tr("The selected camera doesn't have an audio input, or it is not configured"
                    " correctly. The audio stream is not available."));
        };

    const auto getOutputString =
        [this]
        {
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("The selected device does not have an audio output, or it is not configured"
                    " correctly. 2-way audio is not available."),
                tr("The selected camera does not have an audio output, or it is not configured"
                    " correctly. 2-way audio is not available."));
        };

    return *m_streamDirection == Input ? getInputString() : getOutputString();
}

QString AudioRedirectPickerWidget::differentServersErrorText() const
{
    if (!NX_ASSERT(m_streamDirection, "Audio redirect picker not initialized"))
        return QString();

    if (*m_streamDirection == Output)
        return QString();

    return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
        tr("Selected device is connected to another server. Audio stream is not available."),
        tr("Selected camera is connected to another server. Audio stream is not available."));
}

} // namespace nx::vms::client::desktop
