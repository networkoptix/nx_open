#include "camera_list_model.h"
#include "core/resource/camera_resource.h"
#include "core/resource/resource_type.h"
#include "ui/style/resource_icon_cache.h"
#include "ui/workbench/workbench_context.h"
#include "ui/common/recording_status_helper.h"
#include "core/resource/media_server_resource.h"

QnCameraListModel::QnCameraListModel(QnWorkbenchContext* context, QObject *parent):
    m_context(context),
    QnResourceListModel(parent)
{

}

QVariant QnCameraListModel::data(const QModelIndex &index, int role) const
{
    QVariant result;

    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnVirtualCameraResourcePtr &camera = m_resources[index.row()].dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return QVariant();

    switch(role) {
    case Qt::DecorationRole:
        if (index.column() == RecordingColumn) {
            int recordingMode = camera->isScheduleDisabled() ? -1 : Qn::RecordingType_Never;
            if(camera->getStatus() == QnResource::Recording)
                recordingMode =  QnRecordingStatusHelper::currentRecordingMode(m_context, camera);
            result =  QnRecordingStatusHelper::icon(recordingMode);
        }
        else if (index.column() == NameColumn)
            result = qnResIconCache->icon(camera->flags(), camera->getStatus());
        else if (index.column() == ServerColumn)
            result = qnResIconCache->icon(camera->getParentResource()->flags(), camera->getParentResource()->getStatus());
        break;
    case Qt::DisplayRole:
        switch ((Column) index.column())
        {
            case RecordingColumn: {
                int recordingMode = camera->isScheduleDisabled() ? -1 : Qn::RecordingType_Never;
                if(camera->getStatus() == QnResource::Recording)
                    recordingMode =  QnRecordingStatusHelper::currentRecordingMode(m_context, camera);
                result =  QnRecordingStatusHelper::shortTooltip(recordingMode);
                break;
            }
            case NameColumn:
                result = camera->getName();
                break;
            case VendorColumn: {
                QnResourceTypePtr resourceType = qnResTypePool->getResourceType(camera->getTypeId());
                result = resourceType ? QVariant(resourceType->getManufacture()) : QVariant();
                break;
            }
            case ModelColumn:
                result = camera->getModel();
                break;
            case FirmwareColumn:
                result = camera->getFirmware();
                break;
            case IPColumn:
                result = camera->getHostAddress();
                break;
            case UniqIdColumn:
                result = camera->getPhysicalId();
                break;
            case ServerColumn: {
                QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
                if (server)
                    result = QString(lit("%1 (%2)")).arg(server->getName()).arg(QUrl(server->getApiUrl()).host());
                break;
            }
            default:
                result = QnResourceListModel::data(index, role);
        }
        break;
    default:
        result = QnResourceListModel::data(index, role);
    }

    return result;
}

QString QnCameraListModel::columnTitle(Column column) const
{
    switch(column) {
    case RecordingColumn: return tr("Recording");
    case NameColumn:      return tr("Name");
    case VendorColumn:    return tr("Manufacturer");
    case ModelColumn:     return tr("Model");
    case FirmwareColumn:  return tr("Firmware");
    case IPColumn:        return tr("IP/Name");
    case UniqIdColumn:    return tr("ID/MAC");
    case ServerColumn:    return tr("Server");
    default:
        assert(false);
        return QString();
    }
}

void QnCameraListModel::setColumns(const QList<Column>& columns) 
{
    m_columns = columns;
}

int QnCameraListModel::columnCount(const QModelIndex &parent) const
{
    return m_columns.size();
}

QVariant QnCameraListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return columnTitle(m_columns[section]);
    return QnResourceListModel::headerData(section, orientation, role);
}
