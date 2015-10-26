#include "backup_cameras_dialog.h"


#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/common/palette.h>
#include <ui/delegates/backup_cameras_resource_model_delegate.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_pool_model.h>

namespace {

    const int dialogMinimumWidth = 800;

    class QnBackupCamerasDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;

        Q_DECLARE_TR_FUNCTIONS(QnBackupCamerasDialogDelegate);
    public:
        typedef std::function<void(Qn::CameraBackupQualities)> ButtonCallback;

        QnBackupCamerasDialogDelegate(
            ButtonCallback callback, 
            QnBackupCamerasResourceModelDelegate* customColumnDelegate,
            QWidget* parent):

        base_type(parent)
            , m_placeholder(nullptr)
            , m_callback(callback)
            , m_customColumnDelegate(customColumnDelegate)
        {}

        ~QnBackupCamerasDialogDelegate() 
        {}

        virtual void init(QWidget* parent) override {
            if (!checkFrame(parent))
                return;

            parent->layout()->setSpacing(0);
            parent->layout()->addWidget(initLine(parent));
            m_placeholder = initPlacehoder(parent);
            parent->layout()->addWidget(m_placeholder);

            QHBoxLayout* layout = new QHBoxLayout(m_placeholder);
            m_placeholder->setLayout(layout);

            QLabel* label = new QLabel(tr("Set Priority:"), parent);
            layout->addWidget(label);

            for (int i = 0; i < Qn::CameraBackup_Default; ++i) {
                Qn::CameraBackupQualities qualities = static_cast<Qn::CameraBackupQualities>(i);
                auto button = new QPushButton(QnBackupCamerasDialog::qualitiesToString(qualities), parent);
                setPaletteColor(button, QPalette::Window, qApp->palette().color(QPalette::Window));
                connect(button, &QPushButton::clicked, this, [this, qualities]() { m_callback(qualities); });
                layout->addWidget(button);
            }

            layout->addStretch();
        }

        virtual bool validate(const QnResourceList &selected) override {
            bool visible = !selected.filtered<QnVirtualCameraResource>().isEmpty();

            if (m_placeholder)
                m_placeholder->setVisible(visible);
            return true;
        }

        virtual bool isFlat() const override {
            return true;
        }

        virtual QnResourcePoolModelCustomColumnDelegate* customColumnDelegate() const override {
            return m_customColumnDelegate;
        }

    private:
        bool checkFrame(QWidget* parent) {
            Q_ASSERT_X(m_callback, Q_FUNC_INFO, "Callback must be set here");
            if (!m_callback)
                return false;

            Q_ASSERT_X(parent && parent->layout(), Q_FUNC_INFO, "Invalid delegate frame");
            if (!parent || !parent->layout())
                return false;

            return true;
        }

        QWidget* initLine(QWidget* parent) {
            QFrame* line = new QFrame(parent);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            setPaletteColor(line, QPalette::Window, qApp->palette().color(QPalette::Base));
            line->setAutoFillBackground(true);
            return line;
        }

        QWidget* initPlacehoder(QWidget* parent) {
            QWidget* placeholder = new QWidget(parent);
            placeholder->setContentsMargins(0, 0, 0, 0);
            setPaletteColor(placeholder, QPalette::Window, qApp->palette().color(QPalette::Base));
            placeholder->setAutoFillBackground(true);
            return placeholder;
        }

    private:
        QWidget* m_placeholder;
        ButtonCallback m_callback;
        QnResourcePoolModelCustomColumnDelegate* m_customColumnDelegate;
    };

}


QnBackupCamerasDialog::QnBackupCamerasDialog(QWidget* parent /*= nullptr*/):
    base_type(parent)
    , m_customColumnDelegate(new QnBackupCamerasResourceModelDelegate(this))
{
    //TODO: #GDM "devices"?
    setWindowTitle(tr("Select Cameras to Backup..."));
    setMinimumWidth(dialogMinimumWidth);
    setDelegate(new QnBackupCamerasDialogDelegate([this](Qn::CameraBackupQualities qualities){
        updateQualitiesForSelectedCameras(qualities);
    },
        m_customColumnDelegate,
        this));

    auto connectToCamera = [this](const QnVirtualCameraResourcePtr &camera) {
        connect(camera, &QnVirtualCameraResource::backupQualitiesChanged, m_customColumnDelegate, &QnResourcePoolModelCustomColumnDelegate::notifyDataChanged);
    };

    for (const QnVirtualCameraResourcePtr &camera: qnResPool->getAllCameras(QnResourcePtr(), true)) {
        connectToCamera(camera);
    }

    connect(qnResPool, &QnResourcePool::resourceAdded, this, [connectToCamera](const QnResourcePtr &resource) {
        if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
            connectToCamera(camera);
    });
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr &resource) {
        disconnect(resource, nullptr, this, nullptr);
    });
}

QString QnBackupCamerasDialog::qualitiesToString(Qn::CameraBackupQualities qualities) {
    switch (qualities) {
    case Qn::CameraBackup_Disabled:
        return tr("Disabled", "Cameras Backup");
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

const QnBackupCamerasColors & QnBackupCamerasDialog::colors() const {
    return m_customColumnDelegate->colors();
}

void QnBackupCamerasDialog::setColors(const QnBackupCamerasColors &colors) {
    m_customColumnDelegate->setColors(colors);
}

void QnBackupCamerasDialog::updateQualitiesForSelectedCameras(Qn::CameraBackupQualities qualities) {
    auto modified = selectedResources().filtered<QnVirtualCameraResource>([qualities](const QnVirtualCameraResourcePtr &camera) {
        return camera->getBackupQualities() != qualities;
    });

    if (modified.isEmpty())
        return;

    qnResourcesChangesManager->saveCameras(modified, [qualities](const QnVirtualCameraResourcePtr &camera) {
        camera->setBackupQualities(qualities);
    });
}
