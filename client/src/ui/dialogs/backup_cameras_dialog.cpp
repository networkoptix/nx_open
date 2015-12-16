#include "backup_cameras_dialog.h"

#include <api/global_settings.h>
#include <api/model/backup_status_reply.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <server/server_storage_manager.h>

#include <ui/common/palette.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_pool_model.h>
#include <ui/style/warning_style.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace {

    const int dialogMinimumWidth = 800;

    /** Check if backup is not running on the given server. */
    bool isValidServer(const QnMediaServerResourcePtr &server)
    {
        if (!server)
            return false;
        return qnServerStorageManager->backupStatus(server).state == Qn::BackupState_None;
    }

    /** Check if camera is already backing up - or backup is not running now. */
    bool isValidCamera(const QnVirtualCameraResourcePtr &camera)
    {
        if (!camera)
            return false;

        return camera->getActualBackupQualities() != Qn::CameraBackup_Disabled
            || isValidServer(camera->getParentServer());
    }

    class BackupCamerasDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;

        Q_DECLARE_TR_FUNCTIONS(BackupCamerasDialogDelegate);
    public:
        BackupCamerasDialogDelegate(QWidget* parent)
            : base_type(parent)
            , m_qualityComboBox(new QComboBox(parent))
            , m_backupNewCamerasCheckBox(new QCheckBox(parent))
            , m_warningLabel(new QLabel(parent))
        {
            QList<Qn::CameraBackupQualities> possibleQualities;
            possibleQualities
                << Qn::CameraBackup_HighQuality
                << Qn::CameraBackup_LowQuality
                << Qn::CameraBackup_Both
                ;

            for (Qn::CameraBackupQualities value: possibleQualities)
                m_qualityComboBox->addItem(QnBackupCamerasDialog::qualitiesToString(value), static_cast<int>(value));

            m_backupNewCamerasCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("Backup newly added devices"),
                tr("Backup newly added cameras")
                ));

            setWarningStyle(m_warningLabel);
            m_warningLabel->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("Cannot add new devices while backup process is running."),
                tr("Cannot add new cameras while backup process is running.")
                ));
        }

        ~BackupCamerasDialogDelegate()
        {}

        virtual void init(QWidget* parent) override {
            if (!checkFrame(parent))
                return;

            QWidget* placeholder = new QWidget(parent);
            QHBoxLayout* layout = new QHBoxLayout(placeholder);
            parent->layout()->addWidget(placeholder);
            parent->layout()->addWidget(m_warningLabel);

            layout->addWidget(m_backupNewCamerasCheckBox);
            layout->addStretch();
            layout->addWidget(new QLabel(tr("What to backup:"), placeholder));
            layout->addWidget(m_qualityComboBox);
        }

        virtual bool validate(const QnResourceList &selectedResources) override
        {
            QnVirtualCameraResourceList cameras = selectedResources.filtered<QnVirtualCameraResource>();
            bool valid = boost::algorithm::all_of(cameras, isValidCamera);
            m_warningLabel->setVisible(!valid);
            return valid;
        }

        virtual bool isValid(const QnResourcePtr &resource) const override
        {
            if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
                return isValidServer(server);

            if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
                return isValidCamera(camera);

            return false;
        }

        Qn::CameraBackupQualities quality() const
        {
            return static_cast<Qn::CameraBackupQualities>(m_qualityComboBox->currentData().toInt());
        }

        void setQuality(Qn::CameraBackupQualities value)
        {
            int index = m_qualityComboBox->findData(static_cast<int>(value));
            if (index >= 0)
                m_qualityComboBox->setCurrentIndex(index);
        }

        bool backupNewCameras() const
        {
            return m_backupNewCamerasCheckBox->isChecked();
        }

        void setBackupNewCameras(bool value)
        {
            m_backupNewCamerasCheckBox->setChecked(value);
        }

    private:


        bool checkFrame(QWidget* parent)
        {
            Q_ASSERT_X(parent && parent->layout(), Q_FUNC_INFO, "Invalid delegate frame");
            if (!parent || !parent->layout())
                return false;

            return true;
        }

    private:
        QComboBox *m_qualityComboBox;
        QCheckBox *m_backupNewCamerasCheckBox;
        QLabel *m_warningLabel;
    };
}


QnBackupCamerasDialog::QnBackupCamerasDialog(QWidget* parent /*= nullptr*/)
    : base_type(parent)
{
    const QString title = QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Select Devices to Backup..."),
        tr("Select Cameras to Backup...")
        );

    setWindowTitle(title);
    setMinimumWidth(dialogMinimumWidth);

    bool backupNewCameras = qnGlobalSettings->backupNewCamerasByDefault();

    auto dialogDelegate = new BackupCamerasDialogDelegate(this);
    dialogDelegate->setQuality(qnGlobalSettings->backupQualities());
    dialogDelegate->setBackupNewCameras(backupNewCameras);
    setDelegate(dialogDelegate);

    QnVirtualCameraResourceList selectedCameras = qnResPool->getAllCameras(QnResourcePtr(), true).filtered([](const QnVirtualCameraResourcePtr &camera) {
        return camera->getActualBackupQualities() != Qn::CameraBackup_Disabled;
    });

    setSelectedResources(selectedCameras);
}

QString QnBackupCamerasDialog::qualitiesToString(Qn::CameraBackupQualities qualities) {
    switch (qualities) {
    case Qn::CameraBackup_LowQuality:
        return tr("Low-Res Streams", "Cameras Backup");
    case Qn::CameraBackup_HighQuality:
        return tr("Hi-Res Streams", "Cameras Backup");
    case Qn::CameraBackup_Both:
        return tr("All streams", "Cameras Backup");
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }

    return QString();
}


void QnBackupCamerasDialog::buttonBoxClicked( QDialogButtonBox::StandardButton button ) {
    if (button != QDialogButtonBox::Ok)
        return;

    auto dialogDelegate = dynamic_cast<BackupCamerasDialogDelegate*>(this->delegate());
    bool backupNewCameras = dialogDelegate->backupNewCameras();
    Qn::CameraBackupQualities quality = dialogDelegate->quality();

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
