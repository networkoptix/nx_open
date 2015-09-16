#include "failover_priority_dialog.h"

#include <api/app_server_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>

#include <ui/models/resource_pool_model.h>

namespace {

    class QnFailoverPriorityResourceModelDelegate: public QnResourcePoolModelCustomColumnDelegate {
        typedef QnResourcePoolModelCustomColumnDelegate base_type;
    public:
        QnFailoverPriorityResourceModelDelegate(QObject* parent = nullptr):
            base_type(parent)
        {};

        virtual ~QnFailoverPriorityResourceModelDelegate(){}

        virtual Qt::ItemFlags flags(const QModelIndex &index) const override {
            Q_UNUSED(index);
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
        }

        virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
            QnVirtualCameraResourcePtr camera = index.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return QVariant();

            auto priority = camera->failoverPriority();

            switch (role) {
            case Qt::DisplayRole:
            case Qt::StatusTipRole:
            case Qt::WhatsThisRole:
            case Qt::AccessibleTextRole:
            case Qt::AccessibleDescriptionRole:
            case Qt::ToolTipRole:
                return QnFailoverPriorityDialog::priorityToString(priority);
            case Qt::ForegroundRole:
                switch (priority) {
                case Qn::FP_High:
                    return QBrush(Qt::red);
                case Qn::FP_Medium:
                    return QBrush(Qt::yellow);
                case Qn::FP_Low:
                    return QBrush(Qt::green);
                case Qn::FP_Never:
                    return QBrush(Qt::gray);
                default:
                    break;
                }
            default:
                break;
            }
            return QVariant();
        }

        virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override {
            QN_UNUSED(index, value, role);
            return false;
        }
    };

    class QnFailoverPriorityDialogDelegate: public QnResourceSelectionDialogDelegate {
        typedef QnResourceSelectionDialogDelegate base_type;
    public:
        typedef std::function<void(Qn::FailoverPriority)> ButtonCallback;

        QnFailoverPriorityDialogDelegate(ButtonCallback callback, QWidget* parent):
            base_type(parent),
            m_callback(callback),
            m_customColumnDelegate(new QnFailoverPriorityResourceModelDelegate(this))
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
