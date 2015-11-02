#include "backup_cameras_resource_model_delegate.h"

#include <api/global_settings.h>

#include <core/resource/camera_resource.h>

#include <ui/dialogs/backup_cameras_dialog.h>


QnBackupCamerasResourceModelDelegate::QnBackupCamerasResourceModelDelegate(QObject* parent):
    base_type(parent)
    , m_colors()
{};

QnBackupCamerasResourceModelDelegate::~QnBackupCamerasResourceModelDelegate(){}

Qt::ItemFlags QnBackupCamerasResourceModelDelegate::flags(const QModelIndex &index) const {
    Q_UNUSED(index);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

QVariant QnBackupCamerasResourceModelDelegate::data(const QModelIndex &index, int role) const {
    QnVirtualCameraResourcePtr camera = index.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return QVariant();

    auto qualities = camera->getBackupQualities();

    if (m_forcedQualities.contains(camera->getId()))
        qualities = m_forcedQualities[camera->getId()];
   
    if (qualities == Qn::CameraBackup_Default)
        qualities = qnGlobalSettings->defaultBackupQualities();

    switch (role) {
    case Qt::DisplayRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::ToolTipRole:
        return QnBackupCamerasDialog::qualitiesToString(qualities);
    case Qt::ForegroundRole:
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
    default:
        break;
    }
    return QVariant();
}

bool QnBackupCamerasResourceModelDelegate::setData(const QModelIndex &index, const QVariant &value, int role) {
    QN_UNUSED(index, value, role);
    return false;
}

const QnBackupCamerasColors &QnBackupCamerasResourceModelDelegate::colors() const {
    return m_colors;
}

void QnBackupCamerasResourceModelDelegate::setColors(const QnBackupCamerasColors &colors) {
    m_colors = colors;
    emit notifyDataChanged();
}

void QnBackupCamerasResourceModelDelegate::forceCamerasQualities( const QnVirtualCameraResourceList &cameras, Qn::CameraBackupQualities qualities ) {
    for (const QnVirtualCameraResourcePtr &camera: cameras)
        m_forcedQualities[camera->getId()] = qualities;
    emit notifyDataChanged();
}

QHash<QnUuid, Qn::CameraBackupQualities> QnBackupCamerasResourceModelDelegate::forcedCamerasQualities() const {
    return m_forcedQualities;
}



