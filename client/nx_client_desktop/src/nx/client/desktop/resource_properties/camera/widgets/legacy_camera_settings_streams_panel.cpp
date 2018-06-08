#include "legacy_camera_settings_streams_panel.h"
#include "ui_legacy_camera_settings_streams_panel.h"

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/common/clipboard_button.h>
#include <nx/client/desktop/resource_properties/camera/dialogs/camera_streams_dialog.h>

#include <ui/common/aligner.h>

namespace nx {
namespace client {
namespace desktop {

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

    ui::ClipboardButton::createInline(cameraIdLineEdit, ui::ClipboardButton::StandardType::copy);
    ui::ClipboardButton::createInline(primaryLineEdit, ui::ClipboardButton::StandardType::copy);
    ui::ClipboardButton::createInline(secondaryLineEdit, ui::ClipboardButton::StandardType::copy);

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
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
        && (m_camera->sourceUrl(Qn::CR_LiveVideo) != m_model.primaryStreamUrl
            || m_camera->sourceUrl(Qn::CR_SecondaryLiveVideo) != m_model.secondaryStreamUrl);
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
    m_model = {};

    if (!m_camera)
    {
        ui->cameraIdInputField->setText(QString());
        ui->primaryStreamUrlInputField->setText(QString());
        ui->secondaryStreamUrlInputField->setText(QString());
    }
    else
    {
        ui->cameraIdInputField->setText(m_camera->getId().toSimpleString());
        m_model.primaryStreamUrl = m_camera->sourceUrl(Qn::CR_LiveVideo);
        m_model.secondaryStreamUrl = m_camera->sourceUrl(Qn::CR_SecondaryLiveVideo);
    }

    ui->editStreamsButton->setVisible(m_camera
        && m_camera->hasCameraCapabilities(Qn::CustomMediaUrlCapability));
    updateFields();
}

void LegacyCameraSettingsStreamsPanel::submitToResource()
{
    if (hasChanges())
    {
        m_camera->updateSourceUrl(m_model.primaryStreamUrl, Qn::CR_LiveVideo, /*save*/ false);
        m_camera->updateSourceUrl(m_model.secondaryStreamUrl, Qn::CR_SecondaryLiveVideo, false);
    }
}

void LegacyCameraSettingsStreamsPanel::editCameraStreams()
{
    NX_ASSERT(m_camera);
    if (!m_camera)
        return;

    QScopedPointer<CameraStreamsDialog> dialog(new CameraStreamsDialog(this));
    dialog->setModel(m_model);
    if (!dialog->exec())
        return;

    m_model = dialog->model();
    updateFields();
    emit hasChangesChanged();
}

void LegacyCameraSettingsStreamsPanel::updateFields()
{
    const bool isIoModule = m_camera && m_camera->isIOModule();
    const bool hasAudio = m_camera && m_camera->isAudioSupported();
    const bool hasDualStreaming = m_camera && m_camera->hasDualStreaming2();

    const bool hasPrimaryStream = !isIoModule
        || hasAudio
        || !m_model.primaryStreamUrl.isEmpty();
    ui->primaryStreamUrlInputField->setEnabled(hasPrimaryStream);
    ui->primaryStreamUrlInputField->setText(hasPrimaryStream
        ? m_model.primaryStreamUrl
        : tr("I/O module has no audio stream"));

    const bool hasSecondaryStream = hasDualStreaming || !m_model.secondaryStreamUrl.isEmpty();
    ui->secondaryStreamUrlInputField->setEnabled(hasSecondaryStream);
    ui->secondaryStreamUrlInputField->setText(hasSecondaryStream
        ? m_model.secondaryStreamUrl
        : tr("Camera has no secondary stream"));
}

} // namespace desktop
} // namespace client
} // namespace nx
