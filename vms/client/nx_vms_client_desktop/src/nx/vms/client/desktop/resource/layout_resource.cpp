// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_resource.h"

#include <nx/utils/qset.h>
#include <nx/vms/api/data/layout_data.h>

namespace nx::vms::client::desktop {

qreal LayoutResource::cellSpacingValue(Qn::CellSpacing spacing)
{
    switch (spacing)
    {
        case Qn::CellSpacing::None:
            return 0.0;
        case Qn::CellSpacing::Small:
            return nx::vms::api::LayoutData::kDefaultCellSpacing;
        case Qn::CellSpacing::Medium:
            return nx::vms::api::LayoutData::kDefaultCellSpacing * 2;
        case Qn::CellSpacing::Large:
            return nx::vms::api::LayoutData::kDefaultCellSpacing * 3;
    }
    NX_ASSERT(false, "Unhandled enum value");
    return nx::vms::api::LayoutData::kDefaultCellSpacing;
}

void LayoutResource::setPredefinedCellSpacing(Qn::CellSpacing spacing)
{
    setCellSpacing(cellSpacingValue(spacing));
}

void LayoutResource::cloneItems(LayoutResourcePtr target, ItemsRemapHash* remapHash) const
{
    QnLayoutItemDataList items = m_items->getItems().values();
    ItemsRemapHash newUuidByOldUuid;
    for (int i = 0; i < items.size(); i++)
    {
        QnUuid newUuid = QnUuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for (int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, QnUuid());
    if (remapHash)
        *remapHash = newUuidByOldUuid;

    target->setItems(items);
}

void LayoutResource::cloneTo(LayoutResourcePtr target, ItemsRemapHash* remapHash) const
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        target->setIdUnsafe(QnUuid::createUuid());
        target->setUrl(m_url);
        target->setName(m_name);
        target->setParentId(m_parentId);
        target->setCellSpacing(m_cellSpacing);
        target->setCellAspectRatio(m_cellAspectRatio);
        target->setBackgroundImageFilename(m_backgroundImageFilename);
        target->setBackgroundOpacity(m_backgroundOpacity);
        target->setBackgroundSize(m_backgroundSize);
    }

    cloneItems(target, remapHash);
}

LayoutResourcePtr LayoutResource::clone(ItemsRemapHash* remapHash) const
{
    LayoutResourcePtr result = createClonedInstance();
    cloneTo(result, remapHash);
    return result;
}

QnTimePeriod LayoutResource::localRange() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_localRange;
}

void LayoutResource::setLocalRange(const QnTimePeriod& value)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_localRange = value;
}

LayoutResource::DataHash LayoutResource::data() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_data;
}

void LayoutResource::setData(const DataHash& data)
{
    QSet<Qn::ItemDataRole> updatedRoles;

    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        updatedRoles = nx::utils::toQSet(data.keys() + m_data.keys());
        m_data = data;
    }

    for (const auto role: updatedRoles)
        emit dataChanged(role);
}

QVariant LayoutResource::data(Qn::ItemDataRole role) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    const auto it = m_data.find(role);
    return it != m_data.end()
        ? it.value()
        : QVariant();
}

void LayoutResource::setData(Qn::ItemDataRole role, const QVariant &value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        m_data[role] = value;
    }
    emit dataChanged(role);
}

QVariant LayoutResource::itemData(const QnUuid& id, Qn::ItemDataRole role) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_itemData.value(id).value(role);
}

void LayoutResource::setItemData(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data)
{
    bool notify = false;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        notify = setItemDataUnderLock(id, role, data);
    }
    if (notify)
         emit itemDataChanged(id, role, data);
}

void LayoutResource::cleanupItemData(const QnUuid& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_itemData.remove(id);
}

void LayoutResource::cleanupItemData()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_itemData.clear();
}

bool LayoutResource::isPreviewSearchLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.contains(Qn::LayoutSearchStateRole);
}

bool LayoutResource::isShowreelReviewLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.contains(Qn::LayoutTourUuidRole);
}

bool LayoutResource::isVideoWallReviewLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.contains(Qn::VideoWallResourceRole);
}

bool LayoutResource::setItemDataUnderLock(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    const QVariant& oldData = m_itemData.value(id).value(role);
    if (data.isValid() && oldData == data)
        return false;

    if (data.isValid())
    {
        m_itemData[id][role] = data;
    }
    else
    {
        if (!m_itemData.contains(id))
            return false;

        if (!m_itemData[id].contains(role))
            return false;

        m_itemData[id].remove(role);
        if (m_itemData[id].isEmpty())
            m_itemData.remove(id);
    }

    return true;
}

LayoutResourcePtr LayoutResource::createClonedInstance() const
{
    return LayoutResourcePtr(new LayoutResource());
}

} // namespace nx::vms::client::desktop
