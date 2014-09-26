
#include "layout_resource.h"

#include <list>

#include <utils/common/warnings.h>
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "plugins/resource/avi/avi_resource.h"
#include "core/resource_management/resource_pool.h"


QnLayoutResource::QnLayoutResource(const QnResourceTypePool* resTypePool): 
    base_type(),
    m_cellAspectRatio(-1.0),
    m_cellSpacing(-1.0, -1.0),
    m_userCanEdit(false),
    m_backgroundSize(1, 1),
    m_backgroundOpacity(0.7),
    m_locked(false)
{
    setStatus(Qn::Online, true);
    addFlags(Qn::layout);
    setTypeId(resTypePool->getFixedResourceTypeId(lit("Layout")));
}

QnLayoutResourcePtr QnLayoutResource::clone() const {
    QMutexLocker locker(&m_mutex);

    QnLayoutResourcePtr result(new QnLayoutResource(qnResTypePool));
    result->setId(QnUuid::createUuid());
    result->setName(m_name);
    result->setParentId(m_parentId);
    result->setCellSpacing(m_cellSpacing);
    result->setCellAspectRatio(m_cellAspectRatio);
    result->setBackgroundImageFilename(m_backgroundImageFilename);
    result->setBackgroundOpacity(m_backgroundOpacity);
    result->setBackgroundSize(m_backgroundSize);
    result->setUserCanEdit(m_userCanEdit);

    QnLayoutItemDataList items = m_itemByUuid.values();
    QHash<QnUuid, QnUuid> newUuidByOldUuid;
    for(int i = 0; i < items.size(); i++) {
        QnUuid newUuid = QnUuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for(int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, QnUuid());
    result->setItems(items);

    return result;
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items) {
    QnLayoutItemDataMap map;
    foreach(const QnLayoutItemData &item, items)
        map[item.uuid] = item;
    setItems(map);
}

void QnLayoutResource::setItems(const QnLayoutItemDataMap &items) {
    QMutexLocker locker(&m_mutex);
    setItemsUnderLock(items);
}

void QnLayoutResource::setItemsUnderLock(const QnLayoutItemDataMap &items) {
    foreach(const QnLayoutItemData &item, m_itemByUuid)
        if(!items.contains(item.uuid))
            removeItemUnderLock(item.uuid);

    foreach(const QnLayoutItemData &item, items) {
        if(m_itemByUuid.contains(item.uuid)) {
            updateItemUnderLock(item.uuid, item);
        } else {
            addItemUnderLock(item);
        }
    }
}

QnLayoutItemDataMap QnLayoutResource::getItems() const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid;
}

static QString removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf(QLatin1String("://"));
    return prefix == -1 ? url : url.mid(prefix + 3);
}

void QnLayoutResource::setUrl(const QString& value)
{
    QString oldValue = removeProtocolPrefix(getUrl());
    QnResource::setUrl(value);
    QString newValue = removeProtocolPrefix(getUrl());

#ifdef ENABLE_ARCHIVE
    if (!oldValue.isEmpty() && oldValue != newValue)
    {
        // Local layout renamed
        for(QnLayoutItemDataMap::iterator itr = m_itemByUuid.begin(); itr != m_itemByUuid.end(); ++itr) 
        {
            QnLayoutItemData& item = itr.value();
            item.resource.path = QnLayoutFileStorageResource::updateNovParent(value, item.resource.path);
        }
    }
#endif
}

QnLayoutItemData QnLayoutResource::getItem(const QnUuid &itemUuid) const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid.value(itemUuid);
}

void QnLayoutResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) {
    base_type::updateInner(other, modifiedFields);

    QnLayoutResourcePtr localOther = other.dynamicCast<QnLayoutResource>();
    if(localOther) {
        setItemsUnderLock(localOther->m_itemByUuid);
        m_cellAspectRatio = localOther->m_cellAspectRatio;
        m_cellSpacing = localOther->m_cellSpacing;
        m_userCanEdit = localOther->m_userCanEdit;
        m_backgroundImageFilename = localOther->m_backgroundImageFilename;
        m_backgroundSize = localOther->m_backgroundSize;
        m_backgroundOpacity = localOther->m_backgroundOpacity;
        m_locked = localOther->m_locked;
    }
}

void QnLayoutResource::addItem(const QnLayoutItemData &item) {
    QMutexLocker locker(&m_mutex);
    addItemUnderLock(item);
}

void QnLayoutResource::removeItem(const QnLayoutItemData &item) {
    removeItem(item.uuid);
}

void QnLayoutResource::removeItem(const QnUuid &itemUuid) {
    QMutexLocker locker(&m_mutex);
    removeItemUnderLock(itemUuid);
}

void QnLayoutResource::updateItem(const QnUuid &itemUuid, const QnLayoutItemData &item) {
    QMutexLocker locker(&m_mutex);
    updateItemUnderLock(itemUuid, item);
}

void QnLayoutResource::addItemUnderLock(const QnLayoutItemData &item) {
    if(m_itemByUuid.contains(item.uuid)) {
        qnWarning("Item with UUID %1 is already in this layout resource.", item.uuid.toString());
        return;
    }

    m_itemByUuid[item.uuid] = item;

    m_mutex.unlock();
    emit itemAdded(::toSharedPointer(this), item);
    m_mutex.lock();
}

void QnLayoutResource::updateItemUnderLock(const QnUuid &itemUuid, const QnLayoutItemData &item) {
    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end()) {
        qnWarning("There is no item with UUID %1 in this layout.", itemUuid.toString());
        return;
    }

    if(*pos == item) {
        QHash<int, QVariant>::const_iterator i = item.dataByRole.constBegin();
        while (i != item.dataByRole.constEnd()) {
            pos->dataByRole[i.key()] = i.value();
            ++i;
        }
        return;
    }

    *pos = item;

    m_mutex.unlock();
    emit itemChanged(::toSharedPointer(this), item);
    m_mutex.lock();
}

void QnLayoutResource::removeItemUnderLock(const QnUuid &itemUuid) {
    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end())
        return;

    std::list<QnLayoutItemData> removedItems;

    //removing associated zoom items
    for( auto it = m_itemByUuid.begin(); it != m_itemByUuid.end(); )
    {
        if( it->zoomTargetUuid == itemUuid )
        {
            removedItems.push_front( *it );
            it = m_itemByUuid.erase( it );
        }
        else
        {
            ++it;
        }
    }

    removedItems.push_back( *pos );
    m_itemByUuid.erase(pos);

    m_mutex.unlock();
    for( auto item: removedItems )
        emit itemRemoved(::toSharedPointer(this), item);
    m_mutex.lock();
}

QnTimePeriod QnLayoutResource::getLocalRange() const
{
    return m_localRange;
}

void QnLayoutResource::setLocalRange(const QnTimePeriod& value)
{
    m_localRange = value;
}

void QnLayoutResource::setData(const QHash<int, QVariant> &dataByRole) {
    QMutexLocker locker(&m_mutex);

    m_dataByRole = dataByRole;
}

void QnLayoutResource::setData(int role, const QVariant &value) {
    QMutexLocker locker(&m_mutex);

    m_dataByRole[role] = value;
}

QHash<int, QVariant> QnLayoutResource::data() const {
    QMutexLocker locker(&m_mutex);

    return m_dataByRole;
}

/********* Properties getters and setters **********/

/********* Cell aspect ratio propert **********/
qreal QnLayoutResource::cellAspectRatio() const {
    QMutexLocker locker(&m_mutex);
    return m_cellAspectRatio;
}

void QnLayoutResource::setCellAspectRatio(qreal cellAspectRatio) {
    {
        QMutexLocker locker(&m_mutex);
        if(qFuzzyCompare(m_cellAspectRatio, cellAspectRatio))
            return;
        m_cellAspectRatio = cellAspectRatio;
    }
    emit cellAspectRatioChanged(::toSharedPointer(this));
}

bool QnLayoutResource::hasCellAspectRatio() const {
    return cellAspectRatio() > 0.0;
}

/********* Cell spacing property **********/
QSizeF QnLayoutResource::cellSpacing() const {
    QMutexLocker locker(&m_mutex);
    return m_cellSpacing;
}

void QnLayoutResource::setCellSpacing(const QSizeF &cellSpacing) {
    {
        QMutexLocker locker(&m_mutex);
        if(qFuzzyEquals(m_cellSpacing, cellSpacing))
            return;
        m_cellSpacing = cellSpacing;
    }
    emit cellSpacingChanged(::toSharedPointer(this));
}

void QnLayoutResource::setCellSpacing(qreal horizontalSpacing, qreal verticalSpacing) {
    setCellSpacing(QSizeF(horizontalSpacing, verticalSpacing));
}

/********* User Can Edit property **********/
bool QnLayoutResource::userCanEdit() const {
    QMutexLocker locker(&m_mutex);
    return m_userCanEdit;
}

void QnLayoutResource::setUserCanEdit(bool value) {
    {
        QMutexLocker locker(&m_mutex);
        if(m_userCanEdit == value)
            return;
        m_userCanEdit = value;
    }

    emit userCanEditChanged(::toSharedPointer(this));
}



/********* Background size property **********/
QSize QnLayoutResource::backgroundSize() const {
    QMutexLocker locker(&m_mutex);
    return m_backgroundSize;
}

void QnLayoutResource::setBackgroundSize(QSize size) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_backgroundSize == size)
            return;
        m_backgroundSize = size;
    }
    emit backgroundSizeChanged(::toSharedPointer(this));
}

/********* Background image id property **********/
QString QnLayoutResource::backgroundImageFilename() const {
    QMutexLocker locker(&m_mutex);
    return m_backgroundImageFilename;
}

void QnLayoutResource::setBackgroundImageFilename(const QString &filename) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_backgroundImageFilename == filename)
            return;
        m_backgroundImageFilename = filename;
    }
    emit backgroundImageChanged(::toSharedPointer(this));
}

/********* Background opacity property **********/
qreal QnLayoutResource::backgroundOpacity() const {
    QMutexLocker locker(&m_mutex);
    return m_backgroundOpacity;
}

void QnLayoutResource::setBackgroundOpacity(qreal value) {
    {
        qreal bound = qBound<qreal>(0.0, value, 1.0);
        QMutexLocker locker(&m_mutex);
        if (qFuzzyCompare(m_backgroundOpacity, bound))
            return;
        m_backgroundOpacity = bound;
    }
    emit backgroundOpacityChanged(::toSharedPointer(this));
}

/********* Locked property **********/
bool QnLayoutResource::locked() const {
    QMutexLocker locker(&m_mutex);
    return m_locked;
}

void QnLayoutResource::setLocked(bool value) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_locked == value)
            return;
        m_locked = value;
    }
    emit lockedChanged(::toSharedPointer(this));
}
