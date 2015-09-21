#include "failover_priority_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/common/palette.h>
#include <ui/delegates/failover_priority_resource_model_delegate.h>
#include <ui/models/resource_pool_model.h>

namespace {

    const int dialogMinimumWidth = 800;

    class QnFailoverPriorityDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;

        Q_DECLARE_TR_FUNCTIONS(QnFailoverPriorityDialogDelegate);
    public:
        typedef std::function<void(Qn::FailoverPriority)> ButtonCallback;

        QnFailoverPriorityDialogDelegate(
            ButtonCallback callback, 
            QnFailoverPriorityResourceModelDelegate* customColumnDelegate,
            QWidget* parent):

            base_type(parent)
            , m_placeholder(nullptr)
            , m_callback(callback)
            , m_customColumnDelegate(customColumnDelegate)
        {}

        ~QnFailoverPriorityDialogDelegate() 
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

            for (int i = 0; i < Qn::FP_Count; ++i) {
                Qn::FailoverPriority priority = static_cast<Qn::FailoverPriority>(i);
                auto button = new QPushButton(QnFailoverPriorityDialog::priorityToString(priority), parent);
                setPaletteColor(button, QPalette::Window, qApp->palette().color(QPalette::Window));
                connect(button, &QPushButton::clicked, this, [this, priority]() { m_callback(priority); });
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


QnFailoverPriorityDialog::QnFailoverPriorityDialog(QWidget* parent /*= nullptr*/):
    base_type(parent)
    , m_customColumnDelegate(new QnFailoverPriorityResourceModelDelegate(this))
{
    setWindowTitle(tr("Failover Priority"));
    setMinimumWidth(dialogMinimumWidth);
    setDelegate(new QnFailoverPriorityDialogDelegate([this](Qn::FailoverPriority priority){
            updatePriorityForSelectedCameras(priority);
        },
        m_customColumnDelegate,
        this));


    auto connectToCamera = [this](const QnVirtualCameraResourcePtr &camera) {
        connect(camera, &QnVirtualCameraResource::failoverPriorityChanged, m_customColumnDelegate, &QnResourcePoolModelCustomColumnDelegate::notifyDataChanged);
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

QString QnFailoverPriorityDialog::priorityToString(Qn::FailoverPriority priority) {
    switch (priority) {
    case Qn::FP_Never:
        return tr("Never", "Failover priority");
    case Qn::FP_Low:
        return tr("Low", "Failover priority");
    case Qn::FP_Medium:
        return tr("Medium", "Failover priority");
    case Qn::FP_High:
        return tr("High", "Failover priority");
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }
    return QString();
}

const QnFailoverPriorityColors & QnFailoverPriorityDialog::colors() const {
    return m_customColumnDelegate->colors();
}

void QnFailoverPriorityDialog::setColors(const QnFailoverPriorityColors &colors) {
    m_customColumnDelegate->setColors(colors);
}


void QnFailoverPriorityDialog::updatePriorityForSelectedCameras(Qn::FailoverPriority priority) {
    auto modified = selectedResources().filtered<QnVirtualCameraResource>([priority](const QnVirtualCameraResourcePtr &camera) {
        return camera->failoverPriority() != priority;
    });

    if (modified.isEmpty())
        return;

    //TODO: #GDM SafeMode
    qnResourcesChangesManager->saveCameras(modified, [priority](const QnVirtualCameraResourcePtr &camera) {
        camera->setFailoverPriority(priority);
    });
}
