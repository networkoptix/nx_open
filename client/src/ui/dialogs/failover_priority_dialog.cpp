#include "failover_priority_dialog.h"

#include <api/app_server_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>

namespace {

    class QnFailoverPriorityDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;
    public:
        typedef std::function<void(Qn::FailoverPriority)> ButtonCallback;

        QnFailoverPriorityDialogDelegate(ButtonCallback callback, QWidget* parent):
            base_type(parent),
            m_callback(callback)
        {}

        ~QnFailoverPriorityDialogDelegate() 
        {}

        virtual void init(QWidget* parent) override {
            QWidget* placeholder = new QWidget(parent);
            QHBoxLayout* layout = new QHBoxLayout(placeholder);
            placeholder->setLayout(layout);

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

            parent->layout()->addWidget(placeholder);
        }

        virtual bool validate(const QnResourceList &selected) override {
            bool visible = !selected.filtered<QnVirtualCameraResource>().isEmpty();
            for (auto button: m_priorityButtons)
                button->setVisible(visible);
            return true;
        }


    private:
        std::array<QPushButton*, Qn::FP_Count> m_priorityButtons;
        ButtonCallback m_callback;
    };
}

QnFailoverPriorityDialog::QnFailoverPriorityDialog(QWidget* parent /*= nullptr*/):
    base_type(parent)
{
    setWindowTitle(tr("Failover Priority"));
    setDelegate(new QnFailoverPriorityDialogDelegate([this](Qn::FailoverPriority priority){
        updatePriorityForSelectedCameras(priority);
    }, this));
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

void QnFailoverPriorityDialog::updatePriorityForSelectedCameras(Qn::FailoverPriority priority) {
    auto cameras = selectedResources().filtered<QnVirtualCameraResource>();
    QnVirtualCameraResourceList modified;
    for (auto camera: cameras) {
        if (camera->failoverPriority() == priority)
            continue;
        camera->setFailoverPriority(priority);
        modified << camera;
    }

    if (modified.isEmpty())
        return;

    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->saveUserAttributes(
        QnCameraUserAttributePool::instance()->getAttributesList(idListFromResList(modified)),
        this,
        []{});
}
