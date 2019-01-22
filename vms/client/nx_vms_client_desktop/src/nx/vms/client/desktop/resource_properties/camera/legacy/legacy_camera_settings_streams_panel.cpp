#include "legacy_camera_settings_streams_panel.h"
#include "ui_legacy_camera_settings_streams_panel.h"

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>

#include <nx/vms/client/desktop/resource_properties/camera/dialogs/camera_streams_dialog.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>

namespace nx::vms::client::desktop {

LegacyCameraSettingsStreamsPanel::LegacyCameraSettingsStreamsPanel(QWidget* parent):
    base_type(parent),
    ui(new Ui::LegacyCameraSettingsStreamsPanel())
{
    ui->setupUi(this);

    ui->cameraIdInputField->setReadOnly(true);

    /* ui->primaryStreamUrlInputField->setTitle() is called from updateFromResource() */
    ui->primaryStreamUrlInputField->setReadOnly(true);

    ui->secondaryStreamUrlInputField->setTitle(tr("Secondary Stream"));
    ui->secondaryStreamUrlInputField->setReadOnly(true);

    auto cameraIdLineEdit = ui->cameraIdInputField->findChild<QLineEdit*>();
    auto primaryLineEdit = ui->primaryStreamUrlInputField->findChild<QLineEdit*>();
    auto secondaryLineEdit = ui->secondaryStreamUrlInputField->findChild<QLineEdit*>();
    NX_ASSERT(cameraIdLineEdit && primaryLineEdit && secondaryLineEdit);

    ClipboardButton::createInline(cameraIdLineEdit, ClipboardButton::StandardType::copy);
    ClipboardButton::createInline(primaryLineEdit, ClipboardButton::StandardType::copy);
    ClipboardButton::createInline(secondaryLineEdit, ClipboardButton::StandardType::copy);

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->cameraIdInputField,
        ui->primaryStreamUrlInputField,
        ui->secondaryStreamUrlInputField });

    ui->editStreamsButton->setVisible(false);

    connect(ui->editStreamsButton, &QPushButton::clicked, this,
        &LegacyCameraSettingsStreamsPanel::editCameraStreams);
}

LegacyCameraSettingsStreamsPanel::~LegacyCameraSettingsStreamsPanel() = default;

QnVirtualCameraResourcePtr LegacyCameraSettingsStreamsPanel::camera() const
{
    return m_camera;
}

void LegacyCameraSettingsStreamsPanel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_camera = camera;
}

bool LegacyCameraSettingsStreamsPanel::hasChanges() const
{
    return m_camera
        && m_camera->hasCameraCapabilities(Qn::CustomMediaUrlCapability)
        && (m_camera->sourceUrl(Qn::CR_LiveVideo) != m_streams.primaryStreamUrl
            || m_camera->sourceUrl(Qn::CR_SecondaryLiveVideo) != m_streams.secondaryStreamUrl);
}

void LegacyCameraSettingsStreamsPanel::updateFromResource()
{
    const bool isIoModule = m_camera && m_camera->isIOModule();

    ui->cameraIdInputField->setTitle(isIoModule
        ? tr("I/O Module ID")
        : tr("Camera ID"));

        ui->primaryStreamUrlInputField->setTitle(isIoModule
        ? tr("Audio Stream")
        : tr("Primary Stream"));

    ui->secondaryStreamUrlInputField->setHidden(isIoModule);

    const QString urlPlaceholder = isIoModule
        ? tr("URL is not available. Open stream and try again.")
        : tr("URL is not available. Open video stream and try again.");

    ui->primaryStreamUrlInputField->setPlaceholderText(urlPlaceholder);
    ui->secondaryStreamUrlInputField->setPlaceholderText(urlPlaceholder);
    m_streams = {};

    if (!m_camera)
    {
        ui->cameraIdInputField->setText(QString());
        ui->primaryStreamUrlInputField->setText(QString());
        ui->secondaryStreamUrlInputField->setText(QString());
    }
    else
    {
        ui->cameraIdInputField->setText(m_camera->getId().toSimpleString());
        m_streams.primaryStreamUrl = m_camera->sourceUrl(Qn::CR_LiveVideo);
        m_streams.secondaryStreamUrl = m_camera->sourceUrl(Qn::CR_SecondaryLiveVideo);
    }

    ui->editStreamsButton->setVisible(m_camera
        && m_camera->hasCameraCapabilities(Qn::CustomMediaUrlCapability));
    updateFields();
}

void LegacyCameraSettingsStreamsPanel::submitToResource()
{
    if (hasChanges())
    {
        m_camera->updateSourceUrl(m_streams.primaryStreamUrl, Qn::CR_LiveVideo, /*save*/ false);
        m_camera->updateSourceUrl(m_streams.secondaryStreamUrl, Qn::CR_SecondaryLiveVideo, false);
    }
}

void LegacyCameraSettingsStreamsPanel::editCameraStreams()
{
    NX_ASSERT(m_camera);
    if (!m_camera)
        return;

    QScopedPointer<CameraStreamsDialog> dialog(new CameraStreamsDialog(this));
    dialog->setStreams(m_streams);
    if (!dialog->exec())
        return;

    m_streams = dialog->streams();
    updateFields();
    emit hasChangesChanged();
}

void LegacyCameraSettingsStreamsPanel::updateFields()
{
    const bool isIoModule = m_camera && m_camera->isIOModule();
    const bool hasAudio = m_camera && m_camera->isAudioSupported();
    const bool hasDualStreaming = m_camera && m_camera->hasDualStreaming();

    const bool hasPrimaryStream = !isIoModule
        || hasAudio
        || !m_streams.primaryStreamUrl.isEmpty();
    ui->primaryStreamUrlInputField->setEnabled(hasPrimaryStream);
    ui->primaryStreamUrlInputField->setText(hasPrimaryStream
        ? m_streams.primaryStreamUrl
        : tr("I/O module has no audio stream"));

    const bool hasSecondaryStream = hasDualStreaming || !m_streams.secondaryStreamUrl.isEmpty();
    ui->secondaryStreamUrlInputField->setEnabled(hasSecondaryStream);
    ui->secondaryStreamUrlInputField->setText(hasSecondaryStream
        ? m_streams.secondaryStreamUrl
        : tr("Camera has no secondary stream"));
}

} // namespace nx::vms::client::desktop
