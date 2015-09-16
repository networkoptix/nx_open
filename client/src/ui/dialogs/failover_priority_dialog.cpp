#include "failover_priority_dialog.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>

#include <ui/delegates/failover_priority_resource_model_delegate.h>
#include <ui/models/resource_pool_model.h>

namespace {
    class QnFailoverPriorityDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;

        Q_DECLARE_TR_FUNCTIONS(QnFailoverPriorityDialogDelegate);
    public:
        typedef std::function<void(Qn::FailoverPriority)> ButtonCallback;

        QnFailoverPriorityDialogDelegate(
            ButtonCallback callback, 
            QnFailoverPriorityResourceModelDelegate* customColumnDelegate,
            QWidget* parent):

            base_type(parent),
            m_callback(callback),
            m_customColumnDelegate(customColumnDelegate)
        {}

        ~QnFailoverPriorityDialogDelegate() 
        {}

        virtual void init(QWidget* parent) override {
            QWidget* placeholder = new QWidget(parent);
            QHBoxLayout* layout = new QHBoxLayout(placeholder);
            placeholder->setLayout(layout);

            layout->addWidget(new QLabel(tr("Set Priority:"), parent));

            for (int i = 0; i < Qn::FP_Count; ++i) {
                Qn::FailoverPriority priority = static_cast<Qn::FailoverPriority>(i);
                auto button = new QPushButton(QnFailoverPriorityDialog::priorityToString(priority), parent);
                m_priorityButtons[i] = button;
                layout->addWidget(button);
                if (m_callback)
                    connect(button, &QPushButton::clicked, this, [this, priority](){
                        m_callback(priority);
                });
            }
            layout->addStretch();

            parent->layout()->addWidget(placeholder);
        }

        virtual bool validate(const QnResourceList &selected) override {
            bool visible = !selected.filtered<QnVirtualCameraResource>().isEmpty();
            for (auto button: m_priorityButtons)
                button->setVisible(visible);
            return true;
        }

        virtual QnResourcePoolModelCustomColumnDelegate* customColumnDelegate() const override {
            return m_customColumnDelegate;
        }

    private:
        std::array<QPushButton*, Qn::FP_Count> m_priorityButtons;
        ButtonCallback m_callback;
        QnResourcePoolModelCustomColumnDelegate* m_customColumnDelegate;
    };
  
}


QnFailoverPriorityDialog::QnFailoverPriorityDialog(QWidget* parent /*= nullptr*/):
    base_type(parent)
    , m_customColumnDelegate(new QnFailoverPriorityResourceModelDelegate(this))
{
    setWindowTitle(tr("Failover Priority"));
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
        if (QnVirtualCameraResourcePtr &camera = resource.dynamicCast<QnVirtualCameraResource>())
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
    QMap<QnVirtualCameraResourcePtr, Qn::FailoverPriority> backup;

    auto cameras = selectedResources().filtered<QnVirtualCameraResource>();
    QnVirtualCameraResourceList modified;
    for (auto camera: cameras) {
        if (camera->failoverPriority() == priority)
            continue;
        backup[camera] = camera->failoverPriority();
        camera->setFailoverPriority(priority);
        modified << camera;
    }

    if (modified.isEmpty())
        return;

    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->saveUserAttributes(
        QnCameraUserAttributePool::instance()->getAttributesList(idListFromResList(modified)), this, [backup]( int reqID, ec2::ErrorCode errorCode ) {
            Q_UNUSED(reqID);
            if (errorCode == ec2::ErrorCode::ok)
                return;
            for (auto iter = backup.cbegin(); iter != backup.cend(); ++iter) 
                iter.key()->setFailoverPriority(iter.value());
    });
}
