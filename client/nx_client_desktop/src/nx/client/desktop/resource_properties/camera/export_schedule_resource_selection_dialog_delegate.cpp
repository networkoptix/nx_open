#include "export_schedule_resource_selection_dialog_delegate.h"

#include <QtWidgets/QLayout>

#include <ui/style/custom_style.h>
#include <utils/license_usage_helper.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

namespace nx {
namespace client {
namespace desktop {

ExportScheduleResourceSelectionDialogDelegate::ExportScheduleResourceSelectionDialogDelegate(
    QWidget* parent,
    bool recordingEnabled,
    bool motionUsed,
    bool dualStreamingUsed,
    bool hasVideo):
    base_type(parent),
    m_recordingEnabled(recordingEnabled),
    m_motionUsed(motionUsed),
    m_dualStreamingUsed(dualStreamingUsed),
    m_hasVideo(hasVideo)
{
}

bool ExportScheduleResourceSelectionDialogDelegate::doCopyArchiveLength() const
{
    return m_archiveCheckbox->isChecked();
}

void ExportScheduleResourceSelectionDialogDelegate::init(QWidget* parent)
{
    m_parentPalette = parent->palette();

    m_archiveCheckbox = new QCheckBox(parent);
    m_archiveCheckbox->setText(tr("Copy archive length settings"));
    parent->layout()->addWidget(m_archiveCheckbox);

    m_licensesLabel = new QLabel(parent);
    parent->layout()->addWidget(m_licensesLabel);

    auto addWarningLabel =
        [parent](const QString& text) -> QLabel*
        {
            auto label = new QLabel(text, parent);
            setWarningStyle(label);
            parent->layout()->addWidget(label);
            label->setVisible(false);
            return label;
        };

    m_motionLabel = addWarningLabel(tr("Schedule motion type is not supported by some cameras."));
    m_dtsLabel = addWarningLabel(tr("Recording cannot be enabled for some cameras."));
    m_noVideoLabel = addWarningLabel(
        tr("Schedule settings are not compatible with some devices."));
}

bool ExportScheduleResourceSelectionDialogDelegate::validate(const QSet<QnUuid>& selected)
{
    auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(selected);

    QnCamLicenseUsageHelper helper(cameras, m_recordingEnabled, commonModule());

    QPalette palette = m_parentPalette;
    bool licensesOk = helper.isValid();
    QString licenseUsage = helper.getProposedUsageMsg();
    if (!licensesOk)
    {
        setWarningStyle(&palette);
        licenseUsage += L'\n' + helper.getRequiredMsg();
    }
    m_licensesLabel->setText(licenseUsage);
    m_licensesLabel->setPalette(palette);

    bool motionOk = true;
    if (m_motionUsed)
    {
        foreach(const QnVirtualCameraResourcePtr &camera, cameras)
        {
            bool hasMotion = (camera->getMotionType() != Qn::MotionType::MT_NoMotion);
            if (!hasMotion)
            {
                motionOk = false;
                break;
            }
            if (m_dualStreamingUsed && !camera->hasDualStreaming())
            {
                motionOk = false;
                break;
            }
        }
    }
    m_motionLabel->setVisible(!motionOk);

    bool dtsCamPresent = std::any_of(
        cameras.cbegin(), cameras.cend(),
        [](const QnVirtualCameraResourcePtr& camera) { return camera->isDtsBased(); });
    m_dtsLabel->setVisible(dtsCamPresent);

    /* If source camera has no video, allow to copy only to cameras without video. */
    bool videoOk = m_hasVideo || !std::any_of(
        cameras.cbegin(), cameras.cend(),
        [](const QnVirtualCameraResourcePtr& camera) { return camera->hasVideo(0); });
    m_noVideoLabel->setVisible(!videoOk);

    return licensesOk && motionOk && !dtsCamPresent && videoOk;
}

} // namespace desktop
} // namespace client
} // namespace nx
