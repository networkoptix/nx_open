#include "failover_priority_resource_model_delegate.h"

#include <core/resource/camera_resource.h>

#include <ui/dialogs/failover_priority_dialog.h>


QnFailoverPriorityResourceModelDelegate::QnFailoverPriorityResourceModelDelegate(QObject* parent):
    base_type(parent)
    , m_colors()
{};

QnFailoverPriorityResourceModelDelegate::~QnFailoverPriorityResourceModelDelegate(){}

Qt::ItemFlags QnFailoverPriorityResourceModelDelegate::flags(const QModelIndex &index) const {
    Q_UNUSED(index);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

QVariant QnFailoverPriorityResourceModelDelegate::data(const QModelIndex &index, int role) const {
    QnVirtualCameraResourcePtr camera = index.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return QVariant();

    auto priority = camera->failoverPriority();
    if (m_forcedPriorities.contains(camera->getId()))
        priority = m_forcedPriorities[camera->getId()];

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
            return m_colors.high;
        case Qn::FP_Medium:
            return m_colors.medium;
        case Qn::FP_Low:
            return m_colors.low;
        case Qn::FP_Never:
            return m_colors.never;
        default:
            break;
        }
    default:
        break;
    }
    return QVariant();
}

bool QnFailoverPriorityResourceModelDelegate::setData(const QModelIndex &index, const QVariant &value, int role) {
    QN_UNUSED(index, value, role);
    return false;
}

const QnFailoverPriorityColors &QnFailoverPriorityResourceModelDelegate::colors() const {
    return m_colors;
}

void QnFailoverPriorityResourceModelDelegate::setColors(const QnFailoverPriorityColors &colors) {
    m_colors = colors;
    emit notifyDataChanged();
}

void QnFailoverPriorityResourceModelDelegate::forceCamerasPriority( const QnVirtualCameraResourceList &cameras, Qn::FailoverPriority priority ) {
    for (const QnVirtualCameraResourcePtr &camera: cameras)
        m_forcedPriorities[camera->getId()] = priority;
    emit notifyDataChanged();
}

QHash<QnUuid, Qn::FailoverPriority> QnFailoverPriorityResourceModelDelegate::forcedCamerasPriorities() const {
    return m_forcedPriorities;
}

