#include "backup_cameras_dialog.h"

#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/common/palette.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_pool_model.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace {

    const int dialogMinimumWidth = 800;
}

class QnBackupCamerasDialog::QnBackupCamerasDialogDelegate: public QnResourceSelectionDialogDelegate {
    typedef QnResourceSelectionDialogDelegate base_type;

    Q_DECLARE_TR_FUNCTIONS(QnBackupCamerasDialogDelegate);
public:
    QnBackupCamerasDialogDelegate(QWidget* parent)
        : base_type(parent)
        , m_qualityComboBox(new QComboBox(parent))
        , m_backupNewCamerasCheckBox(new QCheckBox(parent))
    {
        QList<Qn::CameraBackupQualities> possibleQualities;
        possibleQualities
            << Qn::CameraBackup_HighQuality
            << Qn::CameraBackup_LowQuality
            << Qn::CameraBackup_Both
            ;

        for (Qn::CameraBackupQualities value: possibleQualities)
            m_qualityComboBox->addItem(QnBackupCamerasDialog::qualitiesToString(value), static_cast<int>(value));

        m_backupNewCamerasCheckBox->setText(tr("Backup newly added cameras"));
    }

    ~QnBackupCamerasDialogDelegate()
    {}

    virtual void init(QWidget* parent) override {
        if (!checkFrame(parent))
            return;

        QWidget* placeholder = new QWidget(parent);
        QHBoxLayout* layout = new QHBoxLayout(placeholder);
        parent->layout()->addWidget(placeholder);

        layout->addWidget(m_backupNewCamerasCheckBox);
        layout->addStretch();
        layout->addWidget(new QLabel(tr("Backup Quality:"), placeholder));
        layout->addWidget(m_qualityComboBox);
    }

    Qn::CameraBackupQualities quality() const {
        return static_cast<Qn::CameraBackupQualities>(m_qualityComboBox->currentData().toInt());
    }

    void setQuality(Qn::CameraBackupQualities value) {
        int index = m_qualityComboBox->findData(static_cast<int>(value));
        if (index >= 0)
            m_qualityComboBox->setCurrentIndex(index);
    }

    bool backupNewCameras() const {
        return m_backupNewCamerasCheckBox->isChecked();
    }

    void setBackupNewCameras(bool value) {
        m_backupNewCamerasCheckBox->setChecked(value);
    }

private:
    bool checkFrame(QWidget* parent) {
        Q_ASSERT_X(parent && parent->layout(), Q_FUNC_INFO, "Invalid delegate frame");
        if (!parent || !parent->layout())
            return false;

        return true;
    }

private:
    QComboBox* m_qualityComboBox;
    QCheckBox* m_backupNewCamerasCheckBox;
};




QnBackupCamerasDialog::QnBackupCamerasDialog(QWidget* parent /*= nullptr*/)
    : base_type(parent)
    , m_delegate(nullptr)
{
    const QString title = QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Select Devices to Backup..."),
        tr("Select Cameras to Backup...")
        );

    setWindowTitle(title);
    setMinimumWidth(dialogMinimumWidth);

    bool backupNewCameras = qnGlobalSettings->backupNewCamerasByDefault();

    m_delegate = new QnBackupCamerasDialogDelegate(this);
    m_delegate->setQuality(qnGlobalSettings->backupQualities());
    m_delegate->setBackupNewCameras(backupNewCameras);
    setDelegate(m_delegate);


    QnVirtualCameraResourceList selectedCameras = qnResPool->getAllCameras(QnResourcePtr(), true).filtered([backupNewCameras](const QnVirtualCameraResourcePtr &camera) {
        if (camera->getBackupQualities() == Qn::CameraBackup_Default)
            return backupNewCameras;
        return camera->getBackupQualities() != Qn::CameraBackup_Disabled;
    });
    setSelectedResources(selectedCameras);
}

QString QnBackupCamerasDialog::qualitiesToString(Qn::CameraBackupQualities qualities) {
    switch (qualities) {
    case Qn::CameraBackup_LowQuality:
        return tr("Low", "Cameras Backup");
    case Qn::CameraBackup_HighQuality:
        return tr("High", "Cameras Backup");
    case Qn::CameraBackup_Both:
        return tr("High + Low", "Cameras Backup");
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }

    return QString();
}


void QnBackupCamerasDialog::buttonBoxClicked( QDialogButtonBox::StandardButton button ) {
    if (button != QDialogButtonBox::Ok)
        return;

    bool backupNewCameras = m_delegate->backupNewCameras();
    Qn::CameraBackupQualities quality = m_delegate->quality();

    qnGlobalSettings->setBackupNewCamerasByDefault(backupNewCameras);
    qnGlobalSettings->setBackupQualities(quality);

    QnVirtualCameraResourceList selected = selectedResources().filtered<QnVirtualCameraResource>();

    auto qualityForCamera = [selected, quality](const QnVirtualCameraResourcePtr &camera) {
        return selected.contains(camera)
            ? quality
            : Qn::CameraBackup_Disabled;
    };

    /* Update all default cameras and all cameras that we have changed. */
    auto modified = qnResPool->getAllCameras(QnResourcePtr(), true).filtered([qualityForCamera](const QnVirtualCameraResourcePtr &camera) {
        return camera->getBackupQualities() != qualityForCamera(camera);
    });

    if (modified.isEmpty())
        return;

    qnResourcesChangesManager->saveCameras(modified, [qualityForCamera](const QnVirtualCameraResourcePtr &camera) {
        camera->setBackupQualities(qualityForCamera(camera));
    });

}
