// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_item_model.h"

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

namespace nx::vms::client::desktop {

static const QColor klight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{klight16Color, "light16"}}},
    {QIcon::Active, {{klight16Color, "light17"}}},
    {QIcon::Selected, {{klight16Color, "light15"}}},
};

CameraHotspotsItemModel::CameraHotspotsItemModel(QnResourcePool* resourcePool, QObject* parent):
    base_type(parent),
    m_resourcePool(resourcePool)
{
    NX_ASSERT(m_resourcePool);
}

CameraHotspotsItemModel::~CameraHotspotsItemModel()
{
}

void CameraHotspotsItemModel::setHotspots(const common::CameraHotspotDataList& hotspots)
{
    if (m_hotspots == hotspots)
        return;

    beginResetModel();
    m_hotspots = hotspots;
    endResetModel();
}

int CameraHotspotsItemModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : Column::ColumnCount;
}

QVariant CameraHotspotsItemModel::data(const QModelIndex& index, int role) const
{
    const auto hotspotData = m_hotspots.at(index.row());
    const auto resourceId = hotspotData.targetResourceId;
    const auto resource = m_resourcePool->getResourceById(resourceId);

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case IndexColumn:
                return index.row() + 1;

            case TargetColumn:
                if (resource)
                    return resource->getName();
                else if (!resourceId.isNull())
                    return tr("Target resource does not exist");
                else
                    return tr("Select camera or layout");

            case DeleteButtonColumn:
                return tr("Delete");
        }
    }

    if (role == Qt::DecorationRole)
    {
        switch (index.column())
        {
            case TargetColumn:
                if (resource)
                    return qnResIconCache->icon(resource);
                else if (!resourceId.isNull())
                    return qnResIconCache->icon(QnResourceIconCache::Camera);
                else
                    return qnResIconCache->icon(QnResourceIconCache::Cameras);

            case DeleteButtonColumn:
                return qnSkin->icon("text_buttons/delete_20_deprecated.svg", kIconSubstitutions);
        }
    }

    if (role == Qn::ResourceRole && index.column() == TargetColumn && resource)
        return QVariant::fromValue(resource);

    if (role == Qt::CheckStateRole && index.column() == PointedCheckBoxColumn)
        return hotspotData.hasDirection() ? Qt::Checked : Qt::Unchecked;

    if (role == HotspotCameraIdRole && index.column() == TargetColumn && !resourceId.isNull())
        return QVariant::fromValue(resourceId);

    if (role == HotspotColorRole && index.column() == ColorPaletteColumn)
        return QColor(hotspotData.accentColorName);

    if (role == Qn::ItemMouseCursorRole && index.column() == TargetColumn)
        return static_cast<int>(Qt::PointingHandCursor);

    return {};
}

Qt::ItemFlags CameraHotspotsItemModel::flags(const QModelIndex& index) const
{
    auto result = base_type::flags(index);

    if (index.column() == TargetColumn)
        result.setFlag(Qt::ItemIsEditable);

    return result;
}

QVariant CameraHotspotsItemModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Vertical)
        return base_type::headerData(section, orientation, role);

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignLeft;

    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
            case IndexColumn:
                return tr("#");

            case Column::TargetColumn:
                return tr("Target");

            case ColorPaletteColumn:
                return tr("Color");

            case PointedCheckBoxColumn:
                return tr("Pointed");

            case DeleteButtonColumn:
                return {};

            default:
                NX_ASSERT("Unexpected column");
                return {};
        }
    }

    return base_type::headerData(section, orientation, role);
}

QModelIndex CameraHotspotsItemModel::index(int row, int column, const QModelIndex&) const
{
    return createIndex(row, column);
}

int CameraHotspotsItemModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_hotspots.size();
}

QModelIndex CameraHotspotsItemModel::parent(const QModelIndex&) const
{
    return {};
}

} // namespace nx::vms::client::desktop
