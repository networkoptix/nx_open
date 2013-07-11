#include "camera_list_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_type.h>
#include <core/resource/media_server_resource.h>

#include <ui/common/recording_status_helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

QnCameraListModel::QnCameraListModel(QObject *parent, QnWorkbenchContext *context):
    QnResourceListModel(parent),
    QnWorkbenchContextAware(parent, context)
{}

QnCameraListModel::~QnCameraListModel() {
    return;
}

QVariant QnCameraListModel::data(const QModelIndex &index, int role) const
{
    QVariant result;

    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnVirtualCameraResourcePtr &camera = m_resources[index.row()].dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return QVariant();

    Column column = m_columns[index.column()];

    switch(role) {
    case Qt::DecorationRole:
        switch(column) {
        case RecordingColumn: {
            int recordingMode = camera->isScheduleDisabled() ? -1 : Qn::RecordingType_Never;
            recordingMode = QnRecordingStatusHelper::currentRecordingMode(context(), camera);
            result =  QnRecordingStatusHelper::icon(recordingMode);
            break;
        }
        case NameColumn: {
            result = qnResIconCache->icon(camera->flags(), camera->getStatus());
            break;
        }
        case ServerColumn: {
            QnResourcePtr server = camera->getParentResource();
            if (server)
                result = qnResIconCache->icon(server->flags(), server->getStatus());
            break;
        }
        default:
            result = base_type::data(index, role);
            break;
        }
        break;
    case Qt::DisplayRole:
        switch (column) {
            case RecordingColumn: {
                int recordingMode = camera->isScheduleDisabled() ? -1 : Qn::RecordingType_Never;
                recordingMode = QnRecordingStatusHelper::currentRecordingMode(context(), camera);
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
                result = base_type::data(index, role);
        }
        break;
    default:
        result = base_type::data(index, role);
    }

    return result;
}

QString QnCameraListModel::columnTitle(Column column) const
{
    switch(column) {
    case RecordingColumn: return tr("Recording");
    case NameColumn:      return tr("Name");
    case VendorColumn:    return tr("Driver");
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

const QList<QnCameraListModel::Column> &QnCameraListModel::columns() const 
{
    return m_columns;
}

void QnCameraListModel::setColumns(const QList<Column>& columns)
{
    if(m_columns == columns)
        return;

    beginResetModel();
    m_columns = columns;
    endResetModel();
}

int QnCameraListModel::columnCount(const QModelIndex &parent) const
{
    if(!parent.isValid())
        return m_columns.size();

    return 0;
}

QVariant QnCameraListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < m_columns.size())
        return columnTitle(m_columns[section]);
    return QnResourceListModel::headerData(section, orientation, role);
}
