#include "backup_cameras_dialog.h"

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
#include <ui/models/resource/resource_tree_model.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/common/alert_bar.h>
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

    bool isDtsCamera(const QnVirtualCameraResourcePtr &camera)
    {
        return (camera && camera->isDtsBased());
    }

    class BackupCamerasDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;

        Q_DECLARE_TR_FUNCTIONS(BackupCamerasDialogDelegate);
    public:
        BackupCamerasDialogDelegate(QWidget* parent)
            : base_type(parent)
            , m_backupNewCamerasCheckBox(new QCheckBox(parent))
            , m_warningLabel(new QLabel(parent))
        {
            m_backupNewCamerasCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Backup newly added devices"),
                tr("Backup newly added cameras")
                ));

            setWarningStyle(m_warningLabel);
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
        }

        virtual bool validate(const QSet<QnUuid>& selectedResources) override
        {
            static const auto kBackupInProgressWarning = QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Cannot add new devices while backup process is running."),
                tr("Cannot add new cameras while backup process is running."));

            static const auto kDtsWarning = QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Cannot add new devices because they store archive on external storage."),
                tr("Cannot add new cameras because they store archive on external storage."));

            auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(selectedResources);
            if (boost::algorithm::any_of(cameras, isDtsCamera))
            {
                // If has dts-based cameras then change massage accordingly
                m_warningLabel->setText(kDtsWarning);
            }
            else if (!boost::algorithm::all_of(cameras, isValidCamera))
            {
                // If has non-valid cameras then change massage accordingly
                m_warningLabel->setText(kBackupInProgressWarning);
            }
            else
            {
                // Otherwise hide warning
                m_warningLabel->setVisible(false);
                return true;
            }

            m_warningLabel->setVisible(true);
            return false;
        }

        virtual bool isValid(const QnUuid& resourceId) const override
        {
            auto resource = resourcePool()->getResourceById(resourceId);
            if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
                return isValidServer(server);

            if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
                return (isValidCamera(camera) && !isDtsCamera(camera));

            return true;    // In case of groups for encoders
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
            NX_ASSERT(parent && parent->layout(), Q_FUNC_INFO, "Invalid delegate frame");
            if (!parent || !parent->layout())
                return false;

            return true;
        }

    private:
        QCheckBox* m_backupNewCamerasCheckBox;
        QLabel* m_warningLabel;
    };
}

QnBackupCamerasDialog::QnBackupCamerasDialog(QWidget* parent) :
    base_type(QnResourceSelectionDialog::Filter::cameras, parent)
{
    const QString title = QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Select Devices to Backup..."),
        tr("Select Cameras to Backup..."));

    setWindowTitle(title);
    setMinimumWidth(dialogMinimumWidth);

    setDelegate(new BackupCamerasDialogDelegate(this));

    const auto alertBar = new QnPromoBar(this);
    alertBar->setText(tr("If the backup is not set up on the current server of the cameras, "
        "their archive will not be backed up."));

    const auto layout = qobject_cast<QBoxLayout*>(this->layout());
    NX_ASSERT(layout);
    layout->insertWidget(0, alertBar);
}

bool QnBackupCamerasDialog::backupNewCameras() const
{
    return static_cast<const BackupCamerasDialogDelegate*>(delegate())->backupNewCameras();
}

void QnBackupCamerasDialog::setBackupNewCameras(bool value)
{
    static_cast<BackupCamerasDialogDelegate*>(delegate())->setBackupNewCameras(value);
}
