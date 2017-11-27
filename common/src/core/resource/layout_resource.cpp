#include "layout_resource.h"

#include <list>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_filter.h>

#include <utils/common/warnings.h>
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "plugins/resource/avi/avi_resource.h"

QnLayoutResource::QnLayoutResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_items(new QnThreadsafeItemStorage<QnLayoutItemData>(&m_mutex, this)),
    m_cellAspectRatio(-1.0),
    m_cellSpacing(-1.0),
    m_backgroundSize(1, 1),
    m_backgroundOpacity(0.7),
    m_locked(false)
{
    addFlags(Qn::layout);
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kLayoutTypeId));
}

QString QnLayoutResource::getUniqueId() const
{
    if (isFile())
        return getUrl();
    return base_type::getUniqueId();
}

Qn::ResourceStatus QnLayoutResource::getStatus() const
{
    return Qn::Online;
}

QnLayoutResourcePtr QnLayoutResource::clone(QHash<QnUuid, QnUuid>* remapHash) const
{
    QnLayoutResourcePtr result(new QnLayoutResource(commonModule()));

    {
        QnMutexLocker locker(&m_mutex);
        result->setId(QnUuid::createUuid());
        result->setName(m_name);
        result->setParentId(m_parentId);
        result->setCellSpacing(m_cellSpacing);
        result->setCellAspectRatio(m_cellAspectRatio);
        result->setBackgroundImageFilename(m_backgroundImageFilename);
        result->setBackgroundOpacity(m_backgroundOpacity);
        result->setBackgroundSize(m_backgroundSize);
    }

    QnLayoutItemDataList items = m_items->getItems().values();
    QHash<QnUuid, QnUuid> newUuidByOldUuid;
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

    result->setItems(items);
    return result;
}

QnLayoutResourcePtr QnLayoutResource::createFromResource(const QnResourcePtr& resource)
{
    NX_EXPECT(QnResourceAccessFilter::isOpenableInLayout(resource));
    if (!resource)
        return QnLayoutResourcePtr();

    QnLayoutResourcePtr layout(new QnLayoutResource(resource->commonModule()));
    layout->setCellSpacing(0);
    layout->setName(resource->getName());
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        const auto ar = camera->aspectRatio();
        if (ar.isValid())
            layout->setCellAspectRatio(ar.toFloat());
    }

    QnLayoutItemData item;
    item.flags = 0x1; // Layout data item flags is declared in client module. // TODO: #GDM move to api
    item.uuid = QnUuid::createUuid();
    item.combinedGeometry = QRect(0, 0, 1, 1);
    item.resource.id = resource->getId();
    if (resource->hasFlags(Qn::local_media))
        item.resource.uniqueId = resource->getUniqueId();

    QString forcedRotation = resource->getProperty(QnMediaResource::rotationKey());
    if (!forcedRotation.isEmpty())
        item.rotation = forcedRotation.toInt();

    layout->addItem(item);

    return layout;
}

QString QnLayoutResource::toSearchString() const
{
    if (isFile())
        return getName() + L' ' + getUrl();
    return getName();
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items)
{
    QnLayoutItemDataMap map;
    for (QnLayoutItemData item: items)
    {
        // Workaround against corrupted layouts in the database
        if (item.uuid.isNull())
            item.uuid = QnUuid::createUuid();
        map[item.uuid] = item;
    }
    setItems(map);
}

void QnLayoutResource::setItems(const QnLayoutItemDataMap &items)
{
    m_items->setItems(items);
}

QnLayoutItemDataMap QnLayoutResource::getItems() const
{
    return m_items->getItems();
}

void QnLayoutResource::setUrl(const QString& value)
{
    NX_ASSERT(!value.startsWith(lit("layout:")));

    QString oldValue = getUrl();
    QnResource::setUrl(value);

    if (!oldValue.isEmpty() && oldValue != value)
    {
        NX_ASSERT(isFile());
        // Local layout renamed
        for (auto item : m_items->getItems())
        {
            item.resource.uniqueId = QnLayoutFileStorageResource::itemUniqueId(value,
                item.resource.uniqueId);
            m_items->updateItem(item);
        }
    }
}

QnLayoutItemData QnLayoutResource::getItem(const QnUuid &itemUuid) const
{
    return m_items->getItem(itemUuid);
}

Qn::Notifier QnLayoutResource::storedItemAdded(const QnLayoutItemData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemAdded(r, item); };
}

Qn::Notifier QnLayoutResource::storedItemRemoved(const QnLayoutItemData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemRemoved(r, item); };
}

Qn::Notifier QnLayoutResource::storedItemChanged(const QnLayoutItemData& item)
{
    return [r = toSharedPointer(this), item]{ emit r->itemChanged(r, item); };
}

void QnLayoutResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    QnLayoutResourcePtr localOther = other.dynamicCast<QnLayoutResource>();
    if (localOther)
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

        setItemsUnderLockInternal(m_items.data(), localOther->m_items.data(), notifiers);
    }
}

void QnLayoutResource::addItem(const QnLayoutItemData &item)
{
    m_items->addItem(item);
}

void QnLayoutResource::removeItem(const QnLayoutItemData &item)
{
    m_items->removeItem(item);
}

void QnLayoutResource::removeItem(const QnUuid &itemUuid)
{
    m_items->removeItem(itemUuid);
}

void QnLayoutResource::updateItem(const QnLayoutItemData &item)
{
    m_items->updateItem(item);
}

QnTimePeriod QnLayoutResource::getLocalRange() const
{
    return m_localRange;
}

void QnLayoutResource::setLocalRange(const QnTimePeriod& value)
{
    m_localRange = value;
}

void QnLayoutResource::setData(const QHash<int, QVariant> &dataByRole)
{
    QSet<int> updatedRoles;

    {
        QnMutexLocker locker(&m_mutex);

        updatedRoles = (dataByRole.keys() + m_dataByRole.keys()).toSet();
        m_dataByRole = dataByRole;
    }

    for (const auto role: updatedRoles)
        emit dataChanged(role);
}

void QnLayoutResource::setData(int role, const QVariant &value)
{
    {
        QnMutexLocker locker(&m_mutex);

        m_dataByRole[role] = value;
    }
    emit dataChanged(role);
}

QVariant QnLayoutResource::data(int role) const
{
    QnMutexLocker locker(&m_mutex);

    const auto it = m_dataByRole.find(role);
    return it != m_dataByRole.end()
        ? it.value()
        : QVariant();
}

QHash<int, QVariant> QnLayoutResource::data() const
{
    QnMutexLocker locker(&m_mutex);

    return m_dataByRole;
}

/********* Properties getters and setters **********/

/********* Cell aspect ratio propert **********/
float QnLayoutResource::cellAspectRatio() const
{
    QnMutexLocker locker(&m_mutex);
    return m_cellAspectRatio;
}

void QnLayoutResource::setCellAspectRatio(float cellAspectRatio)
{
    {
        QnMutexLocker locker(&m_mutex);
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
    QnMutexLocker locker(&m_mutex);
    return m_cellSpacing;
}

void QnLayoutResource::setCellSpacing(qreal spacing)
{
    {
        QnMutexLocker locker(&m_mutex);
        if(qFuzzyEquals(m_cellSpacing, spacing))
            return;

        m_cellSpacing = spacing;
    }
    emit cellSpacingChanged(::toSharedPointer(this));
}

/********* Background size property **********/
QSize QnLayoutResource::backgroundSize() const
{
    QnMutexLocker locker(&m_mutex);
    return m_backgroundSize;
}

void QnLayoutResource::setBackgroundSize(QSize size)
{
    {
        QnMutexLocker locker(&m_mutex);
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

/********* Background image id property **********/
QString QnLayoutResource::backgroundImageFilename() const
{
    QnMutexLocker locker(&m_mutex);
    return m_backgroundImageFilename;
}

void QnLayoutResource::setBackgroundImageFilename(const QString &filename)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_backgroundImageFilename == filename)
            return;
        m_backgroundImageFilename = filename;
    }
    emit backgroundImageChanged(::toSharedPointer(this));
}

/********* Background opacity property **********/
qreal QnLayoutResource::backgroundOpacity() const
{
    QnMutexLocker locker(&m_mutex);
    return m_backgroundOpacity;
}

void QnLayoutResource::setBackgroundOpacity(qreal value)
{
    {
        qreal bound = qBound<qreal>(0.0, value, 1.0);
        QnMutexLocker locker(&m_mutex);
        if (qFuzzyEquals(m_backgroundOpacity, bound))
            return;
        m_backgroundOpacity = bound;
    }
    emit backgroundOpacityChanged(::toSharedPointer(this));
}

/********* Locked property **********/
bool QnLayoutResource::locked() const
{
    QnMutexLocker locker(&m_mutex);
    return m_locked;
}

void QnLayoutResource::setLocked(bool value)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_locked == value)
            return;
        m_locked = value;
    }
    emit lockedChanged(::toSharedPointer(this));
}

bool QnLayoutResource::isFile() const
{
    NX_EXPECT(hasFlags(Qn::exported_layout) == !getUrl().isEmpty());
    return hasFlags(Qn::exported_layout);
}

bool QnLayoutResource::isShared() const
{
    return getParentId().isNull() && !isFile();
}

bool QnLayoutResource::isServiceLayout() const
{
    static const QString kAutoGeneratedLayoutKey = lit("autoGenerated");

    // Compatibility mode
    if (hasProperty(kAutoGeneratedLayoutKey))
        return true;

    // Shared layouts
    const auto parentId = getParentId();
    if (parentId.isNull())
        return false;

    if (const auto module = commonModule())
    {
        const auto parent = module->resourcePool()->getResourceById(parentId);
        if (parent && parent->hasFlags(Qn::user))
            return false;
    }

    return true;
}

QSet<QnUuid> QnLayoutResource::layoutResourceIds() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items->getItems())
    {
        if (item.uuid.isNull())
            continue;

        result << item.resource.id;
    }
    return result;
}

QSet<QnResourcePtr> QnLayoutResource::layoutResources() const
{
    return layoutResources(resourcePool(), m_items->getItems());
}

QSet<QnResourcePtr> QnLayoutResource::layoutResources(QnResourcePool* resourcePool, const QnLayoutItemDataMap& items)
{
    QSet<QnResourcePtr> result;
    for (const auto& item : items)
    {
        if (item.uuid.isNull())
            continue;

        if (auto resource = resourcePool->getResourceByDescriptor(item.resource))
            result << resource;
    }
    return result;
};
