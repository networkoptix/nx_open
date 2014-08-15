#include "camera_list_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_type.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/recording_status_helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <core/resource/resource_name.h>

QnCameraListModel::QnCameraListModel(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    /* Connect to context. */
    connect(resourcePool(),     SIGNAL(resourceAdded(QnResourcePtr)),   this, SLOT(addCamera(QnResourcePtr)), Qt::QueuedConnection);
    connect(resourcePool(),     SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(removeCamera(QnResourcePtr)), Qt::QueuedConnection);

    QnResourceList resources = resourcePool()->getResources(); 

    /* It is important to connect before iterating as new resources may be added to the pool asynchronously. */
    foreach(const QnResourcePtr &resource, resources)
        addCamera(resource);
}

QnCameraListModel::~QnCameraListModel() {
    return;
}

QModelIndex QnCameraListModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column, (void*)0) : QModelIndex();
}

QModelIndex QnCameraListModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
        return QModelIndex();
}

int QnCameraListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_cameras.size();
    return 0;
}

int QnCameraListModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return ColumnCount;
}

QVariant QnCameraListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() 
        || index.model() != this 
        || index.row() < 0 
        || index.row() >= m_cameras.size())
        return QVariant();

    QnVirtualCameraResourcePtr camera = m_cameras[index.row()];
    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();

    switch(role) {
    case Qt::DecorationRole:
        switch(index.column()) {
        case RecordingColumn: 
            return QnRecordingStatusHelper::icon(QnRecordingStatusHelper::currentRecordingMode(context(), camera));
        case NameColumn: 
            return qnResIconCache->icon(camera);
        case ServerColumn:
            return qnResIconCache->icon(server);
        default:
            break;
        }
        break;
    case Qn::DisplayHtmlRole:
    case Qt::DisplayRole:
        switch (index.column()) {
        case RecordingColumn: 
            return QnRecordingStatusHelper::shortTooltip(QnRecordingStatusHelper::currentRecordingMode(context(), camera));
        case NameColumn:
            return camera->getName();
        case VendorColumn:
            return camera->getVendor();
        case ModelColumn:
            return camera->getModel();
        case FirmwareColumn:
            return camera->getFirmware();
        case IpColumn:
            return camera->getHostAddress();
        case UniqIdColumn:
            return camera->getPhysicalId();
        case ServerColumn: 
            return server ? getFullResourceName(server, true) : QVariant();
        default:
            break;
        }
        break;
    case Qn::ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(camera);
    default:
        break;
    }

    return QVariant();
}

QVariant QnCameraListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        break;
    default:
        return QVariant();
    }

    switch (section) {
    case RecordingColumn: return tr("Recording");
    case NameColumn:      return tr("Name");
    case VendorColumn:    return tr("Vendor");
    case ModelColumn:     return tr("Model");
    case FirmwareColumn:  return tr("Firmware");
    case IpColumn:        return tr("IP/Name");
    case UniqIdColumn:    return tr("ID/MAC");
    case ServerColumn:    return tr("Server");
    default:
        break;
    }

    return QVariant();
}

void QnCameraListModel::setServer(const QnMediaServerResourcePtr & server) {
    if(m_server == server)
        return;

    beginResetModel();
    m_server = server;

    while (!m_cameras.isEmpty())
        removeCamera(m_cameras.first());
    
    QnResourceList resources = resourcePool()->getAllCameras(m_server); 
    foreach(const QnResourcePtr &resource, resources)
        addCamera(resource);

    endResetModel();

    emit serverChanged();
}

QnMediaServerResourcePtr QnCameraListModel::server() const {
    return m_server;
}


void QnCameraListModel::addCamera(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (m_cameras.contains(camera))
        return;

    if (!cameraFits(camera))
        return;

    connect(camera, &QnResource::parentIdChanged, this, &QnCameraListModel::at_resource_parentIdChanged);
    connect(camera, &QnResource::resourceChanged, this, &QnCameraListModel::at_resource_resourceChanged); //TODO: #GDM #Common get rid of resourceChanged

    int row = m_cameras.size();
    beginInsertRows(QModelIndex(), row, row);
    m_cameras << camera;
    endInsertRows();

    //emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

void QnCameraListModel::removeCamera(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;
    int row = m_cameras.indexOf(camera);
    if(row < 0)
        return;

    disconnect(camera, NULL, this, NULL);

    beginRemoveRows(QModelIndex(), row, row);
    m_cameras.removeAt(row);
    endRemoveRows();
}

void QnCameraListModel::at_resource_parentIdChanged(const QnResourcePtr &resource) {
    // ignore parent changes if displaying all cameras
    if (!m_server)
        return;

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (camera->getParentId() == m_server->getId())
        addCamera(camera);
    else
        removeCamera(camera);
}

void QnCameraListModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;
    int row = m_cameras.indexOf(camera);
    if(row < 0)
        return;

    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

bool QnCameraListModel::cameraFits(const QnVirtualCameraResourcePtr &camera) const {
    return camera 
        && !camera->hasFlags(Qn::foreigner)
        && (!m_server  || camera->getParentId() == m_server->getId());
}
