// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_list_model.h"

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_type.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ui/common/recording_status_helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::core;
using namespace nx::vms::client::desktop;

QnCameraListModel::QnCameraListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnCameraListModel::handleResourceAdded, Qt::QueuedConnection);

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnCameraListModel::handleResourceRemoved, Qt::QueuedConnection);

    const auto cameras = resourcePool()->getAllCameras(/*server*/ QnResourcePtr(),
        /*ignoreDesktopCameras*/ true);

    for (const auto& camera: cameras)
        addCamera(camera, /*emitSignals*/ false);
}

QnCameraListModel::~QnCameraListModel()
{
}

QModelIndex QnCameraListModel::index(int row, int column, const QModelIndex& parent) const
{
    return hasIndex(row, column, parent)
        ? createIndex(row, column, nullptr)
        : QModelIndex();
}

QModelIndex QnCameraListModel::parent(const QModelIndex& /*child*/) const
{
    return {};
}

int QnCameraListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_cameras.size();
}

int QnCameraListModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnCount;
}

QVariant QnCameraListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()
        || index.model() != this
        || index.row() < 0
        || index.row() >= m_cameras.size())
    {
        return {};
    }

    const QnVirtualCameraResourcePtr camera = m_cameras[index.row()];
    const QnMediaServerResourcePtr server = camera->getParentServer();

    switch(role)
    {
        case Qt::DecorationRole:
            switch(index.column())
            {
                case RecordingColumn:
                    return RecordingStatusHelper::icon(camera);
                case NameColumn:
                    return qnResIconCache->icon(camera);
                case ServerColumn:
                    return qnResIconCache->icon(server);
                default:
                    break;
            }
            break;

        case Qt::DisplayRole:
            switch (index.column())
            {
                case RecordingColumn:
                    return RecordingStatusHelper::shortTooltip(camera);
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
                case MacColumn:
                    return camera->getMAC().toString();
                case LogicalIdColumn:
                    if (const int logicalId = camera->logicalId(); logicalId > 0)
                        return logicalId;
                    return QVariant();
                case ServerColumn:
                    return server
                        ? QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl)
                        : QVariant();
                default:
                    break;
            }
            break;

        case ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera);

        default:
            break;
    }

    return QVariant();
}

QVariant QnCameraListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            break;
        default:
            return {};
    }

    switch (section)
    {
        case RecordingColumn:
            return tr("Recording");
        case NameColumn:
            return tr("Name");
        case VendorColumn:
            return tr("Vendor");
        case ModelColumn:
            return tr("Model");
        case FirmwareColumn:
            return tr("Firmware");
        case IpColumn:
            return tr("IP/Name");
        case MacColumn:
            return tr("MAC address");
        case LogicalIdColumn:
            if (role == Qt::ToolTipRole)
                return tr("Logical ID");
            return tr("ID");
        case ServerColumn:
            return tr("Server");
        default:
            return {};
    }
}

void QnCameraListModel::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    {
        const ScopedReset reset(this);
        m_server = server;

        for (const auto& camera: m_cameras)
            camera->disconnect(this);

        m_cameras.clear();

        const auto cameras = resourcePool()->getAllCameras(m_server,
            /*ignoreDesktopCameras*/ true);

        for (const auto& camera: cameras)
            addCamera(camera, /*emitSignals*/ false);
    }

    emit serverChanged();
}

QnMediaServerResourcePtr QnCameraListModel::server() const
{
    return m_server;
}

void QnCameraListModel::addCamera(const QnVirtualCameraResourcePtr& camera, bool emitSignals)
{
    if (!NX_ASSERT(camera) || m_cameras.contains(camera) || !cameraFits(camera))
        return;

    connect(camera.get(), &QnResource::parentIdChanged,
        this, &QnCameraListModel::handleParentIdChanged);

    // TODO: #sivanov Get rid of resourceChanged.
    connect(camera.get(), &QnResource::resourceChanged,
        this, &QnCameraListModel::handleResourceChanged);

    const int row = m_cameras.size();
    const ScopedInsertRows insertRows(this, row, row, emitSignals);
    m_cameras << camera;
}

void QnCameraListModel::removeCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera))
        return;

    const int row = m_cameras.indexOf(camera);
    if (row < 0)
        return;

    camera->disconnect(this);

    const ScopedRemoveRows removeRows(this, row, row);
    m_cameras.removeAt(row);
}

void QnCameraListModel::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
        addCamera(camera, /*emitSignals*/ true);
}

void QnCameraListModel::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
        removeCamera(camera);
}

void QnCameraListModel::handleParentIdChanged(const QnResourcePtr& resource)
{
    // Ignore parent changes if displaying all cameras.
    if (!m_server)
        return;

    const auto camera = resource.objectCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (camera->getParentId() == m_server->getId())
        addCamera(camera, /*emitSignals*/ true);
    else
        removeCamera(camera);
}

void QnCameraListModel::handleResourceChanged(const QnResourcePtr& resource)
{
    const auto camera = resource.objectCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    const int row = m_cameras.indexOf(camera);
    if (row >= 0)
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

bool QnCameraListModel::cameraFits(const QnVirtualCameraResourcePtr& camera) const
{
    NX_ASSERT(camera);
    if (!camera || camera->hasFlags(Qn::desktop_camera))
        return false;

    return !m_server || camera->getParentId() == m_server->getId();
}
