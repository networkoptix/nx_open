#include "backup_cameras_dialog.h"

#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/common/palette.h>
#include <ui/delegates/backup_cameras_resource_model_delegate.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_pool_model.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace {

    const int dialogMinimumWidth = 800;
}

//TODO: #GDM move out common code with failover priority dialog to a new dialog class with delegate
class QnBackupCamerasDialog::QnBackupCamerasDialogDelegate: public QnResourceSelectionDialogDelegate {
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
        , m_hintPage(nullptr)
        , m_buttonsPage(nullptr)
        , m_callback(callback)
        , m_colors()
        , m_defaultQualitiesModel(nullptr)
        , m_customColumnDelegate(customColumnDelegate)
        , m_defaultQuality(qnGlobalSettings->defaultBackupQualities())
    {}

    ~QnBackupCamerasDialogDelegate() 
    {}

    const QnBackupCamerasColors &colors() const {
        return m_colors;
    }

    void setColors(const QnBackupCamerasColors &colors) {
        m_colors = colors;
        if (!m_defaultQualitiesModel) 
            return;

        for (int row = 0; row < m_defaultQualitiesModel->rowCount(); ++row) {
            QStandardItem* item = m_defaultQualitiesModel->item(row);
            Qn::CameraBackupQualities qualities = static_cast<Qn::CameraBackupQualities>(item->data().toInt());
            item->setData(getColor(qualities), Qt::ForegroundRole);
        }

    }

    virtual void init(QWidget* parent) override {
        if (!checkFrame(parent))
            return;

        parent->layout()->setSpacing(0);
        parent->layout()->addWidget(initLine(parent));
        m_placeholder = initPlacehoder(parent);
        parent->layout()->addWidget(m_placeholder);
        parent->layout()->addWidget(initDefaultValueWidget(parent));

        QHBoxLayout* layout = new QHBoxLayout(m_buttonsPage);
        QLabel* label = new QLabel(tr("What to backup:"), parent);
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
        if (!m_placeholder)
            return true;

        bool visible = !selected.filtered<QnVirtualCameraResource>().isEmpty();
        m_placeholder->setCurrentWidget(visible
            ? m_buttonsPage
            : m_hintPage);
        return true;
    }

    virtual bool isFlat() const override {
        return true;
    }

    virtual QnResourcePoolModelCustomColumnDelegate* customColumnDelegate() const override {
        return m_customColumnDelegate;
    }

    Qn::CameraBackupQualities defaultQualities() const {
        return m_defaultQuality;
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

    QStandardItemModel* initDefaultQualitiesModel(QObject *parent) {
        QStandardItemModel *model = new QStandardItemModel(parent);
        for (int i = 0; i < Qn::CameraBackup_Default; ++i) {                
            Qn::CameraBackupQualities qualities = static_cast<Qn::CameraBackupQualities>(i);
            QStandardItem *item = new QStandardItem(QnBackupCamerasDialog::qualitiesToString(qualities));
            item->setData(i);
            item->setData(getColor(qualities), Qt::ForegroundRole);
            model->appendRow(item);
        }
        return model;
    }

    QColor getColor(Qn::CameraBackupQualities qualities) {
        switch (qualities) {
        case Qn::CameraBackup_HighQuality:
            return m_colors.high;
        case Qn::CameraBackup_Both:
            return m_colors.both;
        case Qn::CameraBackup_LowQuality:
            return m_colors.low;
        case Qn::CameraBackup_Disabled:
            return m_colors.disabled;
        default:
            break;
        }
        return QColor();
    }

    QWidget* initLine(QWidget* parent) {
        QFrame* line = new QFrame(parent);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        setPaletteColor(line, QPalette::Window, qApp->palette().color(QPalette::Base));
        line->setAutoFillBackground(true);
        return line;
    }

    QStackedWidget* initPlacehoder(QWidget* parent) {
        QStackedWidget* placeholder = new QStackedWidget(parent);
        placeholder->setContentsMargins(0, 0, 0, 0);
        setPaletteColor(placeholder, QPalette::Window, qApp->palette().color(QPalette::Base));
        placeholder->setAutoFillBackground(true);
        
        const QString hint = QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Select devices to setup backup"),
            tr("Select cameras to setup backup")
            );

        m_hintPage = new QLabel(hint, placeholder);
        m_hintPage->setAlignment(Qt::AlignCenter);
        placeholder->addWidget(m_hintPage);
        m_buttonsPage = new QWidget(placeholder);
        placeholder->addWidget(m_buttonsPage);

        return placeholder;
    }

    QWidget* initDefaultValueWidget(QWidget* parent) {
        QWidget* panel = new QWidget(parent);
        QHBoxLayout* layout = new QHBoxLayout(panel);

        const QString title = QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Default value for new devices:"),
            tr("Default value for new cameras:")
            );
        QLabel* caption = new QLabel(title, panel);
        layout->addWidget(caption);

        QComboBox* combobox = new QComboBox(panel);
        layout->addWidget(combobox);
        combobox->setModel(initDefaultQualitiesModel(this));

        combobox->setCurrentIndex(static_cast<int>(m_defaultQuality));
        //TODO: #GDM remove direct index dependency
        connect(combobox, QnComboboxCurrentIndexChanged, this, [this](int index) {
            m_defaultQuality = static_cast<Qn::CameraBackupQualities>(index);
        });
        layout->addWidget(combobox);
        layout->addStretch();

        return panel;
    } 

private:
    QStackedWidget* m_placeholder;
    QLabel* m_hintPage;
    QWidget* m_buttonsPage;

    ButtonCallback m_callback;
    QnBackupCamerasColors m_colors;
    QStandardItemModel *m_defaultQualitiesModel;
    QnResourcePoolModelCustomColumnDelegate* m_customColumnDelegate;
    Qn::CameraBackupQualities m_defaultQuality;
};

const auto watchValidator = [](const QnResourcePtr &resource)
{
    return (resource && resource.dynamicCast<QnVirtualCameraResource>());
};

QnBackupCamerasDialog::QnBackupCamerasDialog(QWidget* parent /*= nullptr*/):
    base_type(watchValidator, tr("There are no cameras to backup"), parent)

    , m_delegate(nullptr)
    , m_customColumnDelegate(new QnBackupCamerasResourceModelDelegate(this))
{
    const QString title = QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Select Devices to Backup..."),
        tr("Select Cameras to Backup...")
        );

    setWindowTitle(title);
    setMinimumWidth(dialogMinimumWidth);

    m_delegate = new QnBackupCamerasDialogDelegate(
        [this](Qn::CameraBackupQualities qualities) {
            updateQualitiesForSelectedCameras(qualities);
        },
        m_customColumnDelegate,
        this);
    setDelegate(m_delegate);

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
        return tr("Nothing", "Cameras Backup");
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
    m_customColumnDelegate->forceCamerasQualities(selectedResources().filtered<QnVirtualCameraResource>(), qualities);
    setSelectedResources(QnResourceList());
}

void QnBackupCamerasDialog::buttonBoxClicked( QDialogButtonBox::StandardButton button ) {
    if (button != QDialogButtonBox::Ok)
        return;

    /* We should update default cameras to old value - what have user seen in the dialog. */
    auto oldDefaultQualities = qnGlobalSettings->defaultBackupQualities();
    qnGlobalSettings->setDefauldBackupQualities(m_delegate->defaultQualities());

    auto forced = m_customColumnDelegate->forcedCamerasQualities();

    /* Update all default cameras and all cameras that we have changed. */
    auto modified = qnResPool->getAllCameras(QnResourcePtr(), true).filtered([this, &forced](const QnVirtualCameraResourcePtr &camera) {
        return forced.contains(camera->getId()) || camera->getBackupQualities() == Qn::CameraBackup_Default;
    });

    if (modified.isEmpty())
        return;

    qnResourcesChangesManager->saveCameras(modified, [this, &forced, oldDefaultQualities](const QnVirtualCameraResourcePtr &camera) {
        if (forced.contains(camera->getId()))
            camera->setBackupQualities(forced[camera->getId()]);
        else if (camera->getBackupQualities() == Qn::CameraBackup_Default)
            camera->setBackupQualities(oldDefaultQualities);
        else
            Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
    });

}
