// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_resource.h"

#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/utils/math/fuzzy.h>

using namespace nx::vms::common;

QnLayoutResource::QnLayoutResource():
    base_type(),
    m_items(new QnThreadsafeItemStorage<LayoutItemData>(&m_mutex, this))
{
    addFlags(Qn::layout);
    setTypeId(nx::vms::api::LayoutData::kResourceTypeId);
}

nx::vms::api::ResourceStatus QnLayoutResource::getStatus() const
{
    return nx::vms::api::ResourceStatus::online;
}

void QnLayoutResource::setStatus(nx::vms::api::ResourceStatus newStatus,
    Qn::StatusChangeReason reason /*= Qn::StatusChangeReason::Local*/)
{
    Q_UNUSED(newStatus);
    Q_UNUSED(reason);
    NX_ASSERT(false, "Not implemented");
}

void QnLayoutResource::setItems(const LayoutItemDataList& items)
{
    LayoutItemDataMap map;
    for (LayoutItemData item: items)
    {
        // Workaround against corrupted layouts in the database
        if (item.uuid.isNull())
            item.uuid = nx::Uuid::createUuid();
        map[item.uuid] = item;
    }
    setItems(map);
}

void QnLayoutResource::setItems(const LayoutItemDataMap &items)
{
    m_items->setItems(items);
}

LayoutItemDataMap QnLayoutResource::getItems() const
{
    return m_items->getItems();
}

void QnLayoutResource::setUrl(const QString& value)
{
    NX_ASSERT(!value.startsWith("layout:"));

    QString oldValue = getUrl();
    QnResource::setUrl(value);

    if (!oldValue.isEmpty() && oldValue != value)
    {
        NX_ASSERT(isFile());
        // Local layout renamed
        for (auto item : m_items->getItems())
        {
            item.resource.path =
                QnLayoutFileStorageResource::itemUniqueId(value, item.resource.path);
            m_items->updateItem(item);
        }
    }
}

LayoutItemData QnLayoutResource::getItem(const nx::Uuid &itemUuid) const
{
    return m_items->getItem(itemUuid);
}

Qn::Notifier QnLayoutResource::storedItemAdded(const LayoutItemData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemAdded(r, item); };
}

Qn::Notifier QnLayoutResource::storedItemRemoved(const LayoutItemData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemRemoved(r, item); };
}

Qn::Notifier QnLayoutResource::storedItemChanged(
    const LayoutItemData& item,
    const LayoutItemData& oldItem)
{
    return [r = toSharedPointer(this), item, oldItem]{ emit r->itemChanged(r, item, oldItem); };
}

void QnLayoutResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    base_type::updateInternal(source, notifiers);

    QnLayoutResourcePtr localOther = source.dynamicCast<QnLayoutResource>();
    if (NX_ASSERT(localOther))
    {
        if (!qFuzzyEquals(m_cellAspectRatio, localOther->m_cellAspectRatio))
        {
            m_cellAspectRatio = localOther->m_cellAspectRatio;
            notifiers << [r = toSharedPointer(this)]{ emit r->cellAspectRatioChanged(r); };
        }

        if (!qFuzzyEquals(m_cellSpacing, localOther->m_cellSpacing))
        {
            m_cellSpacing = localOther->m_cellSpacing;
            notifiers << [r = toSharedPointer(this)]{ emit r->cellSpacingChanged(r); };
        }

        if (m_backgroundImageFilename != localOther->m_backgroundImageFilename)
        {
            m_backgroundImageFilename = localOther->m_backgroundImageFilename;
            notifiers << [r = toSharedPointer(this)]{ emit r->backgroundImageChanged(r); };
        }

        if (m_backgroundSize != localOther->m_backgroundSize)
        {
            m_backgroundSize = localOther->m_backgroundSize;
            notifiers << [r = toSharedPointer(this)]{ emit r->backgroundSizeChanged(r); };
        }

        if (!qFuzzyEquals(m_backgroundOpacity, localOther->m_backgroundOpacity))
        {
            m_backgroundOpacity = localOther->m_backgroundOpacity;
            notifiers << [r = toSharedPointer(this)]{ emit r->backgroundOpacityChanged(r); };
        }

        if (m_locked != localOther->m_locked)
        {
            m_locked = localOther->m_locked;
            notifiers << [r = toSharedPointer(this)]{ emit r->lockedChanged(r); };
        }

        if (m_fixedSize != localOther->m_fixedSize)
        {
            m_fixedSize = localOther->m_fixedSize;
            notifiers << [r = toSharedPointer(this)]{ emit r->fixedSizeChanged(r); };
        }

        if (m_logicalId != localOther->m_logicalId)
        {
            m_logicalId = localOther->m_logicalId;
            notifiers << [r = toSharedPointer(this)]{ emit r->logicalIdChanged(r); };
        }

        setItemsUnderLockInternal(m_items.data(), localOther->m_items.data(), notifiers);
    }
}

void QnLayoutResource::addItem(const LayoutItemData &item)
{
    m_items->addItem(item);
}

void QnLayoutResource::removeItem(const LayoutItemData &item)
{
    m_items->removeItem(item);
}

void QnLayoutResource::removeItem(const nx::Uuid &itemUuid)
{
    m_items->removeItem(itemUuid);
}

void QnLayoutResource::updateItem(const LayoutItemData &item)
{
    m_items->updateItem(item);
}

/********* Properties getters and setters **********/

/********* Cell aspect ratio property **********/
float QnLayoutResource::cellAspectRatio() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_cellAspectRatio;
}

void QnLayoutResource::setCellAspectRatio(float cellAspectRatio)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (qFuzzyEquals(m_cellAspectRatio, cellAspectRatio))
            return;
        m_cellAspectRatio = cellAspectRatio;
    }
    emit cellAspectRatioChanged(::toSharedPointer(this));
}

bool QnLayoutResource::hasCellAspectRatio() const
{
    return cellAspectRatio() > 0.0;
}

/********* Cell spacing property **********/
qreal QnLayoutResource::cellSpacing() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_cellSpacing < 0
        ? nx::vms::api::LayoutData::kDefaultCellSpacing
        : m_cellSpacing;
}

void QnLayoutResource::setCellSpacing(qreal spacing)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (qFuzzyEquals(m_cellSpacing, spacing))
            return;

        m_cellSpacing = spacing;
    }
    emit cellSpacingChanged(::toSharedPointer(this));
}

/********* Background size property **********/
QSize QnLayoutResource::backgroundSize() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_backgroundSize;
}

void QnLayoutResource::setBackgroundSize(QSize size)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_backgroundSize == size)
            return;
        m_backgroundSize = size;
    }
    emit backgroundSizeChanged(::toSharedPointer(this));
}

QRect QnLayoutResource::backgroundRect() const
{
    return backgroundRect(backgroundSize());
}

QRect QnLayoutResource::backgroundRect(const QSize& backgroundSize)
{
    const int left = backgroundSize.width() / 2;
    const int top =  backgroundSize.height() / 2;
    return QRect(-left, -top, backgroundSize.width(), backgroundSize.height());
}

bool QnLayoutResource::hasBackground() const
{
    return !backgroundImageFilename().isEmpty();
}

/********* Background image id property **********/
QString QnLayoutResource::backgroundImageFilename() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_backgroundImageFilename;
}

QSize QnLayoutResource::fixedSize() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_fixedSize;
}

void QnLayoutResource::setFixedSize(const QSize& value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_fixedSize == value)
            return;
        m_fixedSize = value;
    }
    emit fixedSizeChanged(::toSharedPointer(this));
}

int QnLayoutResource::logicalId() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_logicalId;
}

void QnLayoutResource::setLogicalId(int value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_logicalId == value)
            return;
        m_logicalId = value;
    }
    emit logicalIdChanged(::toSharedPointer(this));
}

void QnLayoutResource::setBackgroundImageFilename(const QString &filename)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_backgroundImageFilename == filename)
            return;
        m_backgroundImageFilename = filename;
    }
    emit backgroundImageChanged(::toSharedPointer(this));
}

/********* Background opacity property **********/
qreal QnLayoutResource::backgroundOpacity() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_backgroundOpacity;
}

void QnLayoutResource::setBackgroundOpacity(qreal value)
{
    {
        qreal bound = qBound<qreal>(0.0, value, 1.0);
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (qFuzzyEquals(m_backgroundOpacity, bound))
            return;
        m_backgroundOpacity = bound;
    }
    emit backgroundOpacityChanged(::toSharedPointer(this));
}

/********* Locked property **********/
bool QnLayoutResource::locked() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_locked;
}

void QnLayoutResource::setLocked(bool value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_locked == value)
            return;
        m_locked = value;
    }
    emit lockedChanged(::toSharedPointer(this));
}

bool QnLayoutResource::isFile() const
{
    NX_ASSERT(!hasFlags(Qn::exported_layout));
    return false;
}

bool QnLayoutResource::isShared() const
{
    return getParentId().isNull() && !isFile();
}

bool QnLayoutResource::isServiceLayout() const
{
    static const QString kAutoGeneratedLayoutKey = "autoGenerated";

    // Compatibility mode. Such property was set for Video-Wall-owned Layouts earlier.
    if (!getProperty(kAutoGeneratedLayoutKey).isEmpty())
        return true;

    // Shared layouts.
    const auto parentId = getParentId();
    if (parentId.isNull())
        return false;

    // Service layout can belong to a Server or to a VideoWall.
    const auto parent = getParentResource();
    if (parent)
    {
        if (parent->hasFlags(Qn::user) //< General user's layout.
            || parent->hasFlags(Qn::media)) //< Intercom layout.
        {
            return false;
        }
    }

    return true;
}
