// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "import_from_device_filter_model.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/resource/camera.h>

namespace nx::vms::client::desktop::integrations {

ImportFromDeviceFilterModel::ImportFromDeviceFilterModel(QObject* parent):
    QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setRecursiveFilteringEnabled(true);

    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
}

QString ImportFromDeviceFilterModel::filterWildcard() const
{
    return m_filterWildcard;
}

void ImportFromDeviceFilterModel::setFilterWildcard(const QString& value)
{
    if (m_filterWildcard == value)
        return;

    m_filterWildcard = value;
    emit filterWildcardChanged();
    invalidateFilter();
}

bool ImportFromDeviceFilterModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    return 0 > m_collator.compare(
        sourceModel()->data(sourceLeft, Qt::DisplayRole).toString(),
        sourceModel()->data(sourceRight, Qt::DisplayRole).toString());
}

bool ImportFromDeviceFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);

    const auto resource = qvariant_cast<QnResourcePtr>(
        sourceModel()->data(sourceIndex, core::ResourceRole));
    if (!NX_ASSERT(resource))
        return false;

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(camera))
        return false;

    if (!camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::remoteArchive))
        return false;

    if (filterWildcard().isEmpty())
        return true;

    const QVariant cameraName = sourceModel()->data(sourceIndex, Qt::DisplayRole);
    return cameraName.toString().toLower().contains(filterWildcard().toLower());
}

} // namespace nx::vms::client::desktop::integrations
