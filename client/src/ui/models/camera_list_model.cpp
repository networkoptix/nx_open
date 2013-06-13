#include "camera_list_model.h"
#include "core/resource/camera_resource.h"
#include "core/resource/resource_type.h"

QnCameraListModel::QnCameraListModel(QObject *parent):
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
    case Qt::DisplayRole:
        switch ((Column) index.column())
        {
            case NameColumn:
                return camera->getName();
            case VendorColumn: {
                QnResourceTypePtr resourceType = qnResTypePool->getResourceType(camera->getTypeId());
                return resourceType ? QVariant(resourceType->getManufacture()) : QVariant();
            }
            case ModelColumn:
                return camera->getModel();
            case FirmwareColumn:
                return camera->getFirmware();
            case IPColumn:
                return camera->getHostAddress();
            case UniqIdColumn:
                return camera->getPhysicalId();
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
    case NameColumn:      return tr("Name");
    case VendorColumn:    return tr("Manufacturer");
    case ModelColumn:     return tr("Model");
    case FirmwareColumn:  return tr("Firmware");
    case IPColumn:        return tr("Host");
    case UniqIdColumn:    return tr("ID/MAC");
    default:
        assert(false);
        return QString();
    }
}

void QnCameraListModel::setColumns(const QList<Column>& columns) 
{
    if(m_columns == columns)
        return;

    foreach(Column column, columns) {
        if(column < 0 || column >= ColumnCount) {
            qnWarning("Invalid column '%1'.", static_cast<int>(column));
            return;
        }
    }

    m_columns = columns;
}

int QnCameraListModel::columnCount(const QModelIndex &parent) const
{
    return m_columns.size();
}

QVariant QnCameraListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return columnTitle(m_columns[section]);
    return QnResourceListModel::headerData(section, orientation, role);
}
