#include "layout_resource.h"
#include <utils/common/warnings.h>
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "api/serializer/pb_serializer.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resource_managment/resource_pool.h"

QnLayoutResource::QnLayoutResource(): 
    base_type(),
    m_cellAspectRatio(-1.0),
    m_cellSpacing(-1.0, -1.0),
    m_backgroundSize(1, 1),
    m_backgroundImageId(0),
    m_locked(false)
{
    setStatus(Online, true);
    addFlags(QnResource::layout);
}

void QnLayoutResource::setItems(const QnLayoutItemDataList& items) {
    QnLayoutItemDataMap map;
    foreach(const QnLayoutItemData &item, items)
        map[item.uuid] = item;
    setItems(map);
}

void QnLayoutResource::setItems(const QnLayoutItemDataMap &items) {
    QMutexLocker locker(&m_mutex);

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
    if (!oldValue.isEmpty() && oldValue != newValue)
    {
        // Local layout renamed
        for(QnLayoutItemDataMap::iterator itr = m_itemByUuid.begin(); itr != m_itemByUuid.end(); ++itr) 
        {
            QnLayoutItemData& item = itr.value();
            item.resource.path = updateNovParent(value, item.resource.path);
        }
    }
}

QnLayoutItemData QnLayoutResource::getItem(const QUuid &itemUuid) const {
    QMutexLocker locker(&m_mutex);

    return m_itemByUuid.value(itemUuid);
}

QString QnLayoutResource::getUniqueId() const
{
    return getGuid();
}

void QnLayoutResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnLayoutResourcePtr localOther = other.dynamicCast<QnLayoutResource>();
    if(localOther) {
        setItems(localOther->getItems());
        setCellAspectRatio(localOther->cellAspectRatio());
        setCellSpacing(localOther->cellSpacing());
    }
}

void QnLayoutResource::addItem(const QnLayoutItemData &item) {
    QMutexLocker locker(&m_mutex);
    addItemUnderLock(item);
}

void QnLayoutResource::removeItem(const QnLayoutItemData &item) {
    removeItem(item.uuid);
}

void QnLayoutResource::removeItem(const QUuid &itemUuid) {
    QMutexLocker locker(&m_mutex);
    removeItemUnderLock(itemUuid);
}

void QnLayoutResource::updateItem(const QUuid &itemUuid, const QnLayoutItemData &item) {
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

void QnLayoutResource::updateItemUnderLock(const QUuid &itemUuid, const QnLayoutItemData &item) {
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

void QnLayoutResource::removeItemUnderLock(const QUuid &itemUuid) {
    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end())
        return;

    QnLayoutItemData item = *pos;
    m_itemByUuid.erase(pos);

    m_mutex.unlock();
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

QString QnLayoutResource::updateNovParent(const QString& novName, const QString& itemName)
{
    QString normItemName = itemName.mid(itemName.lastIndexOf(L'?')+1);
    QString normNovName = novName;
    if (!normNovName.startsWith(QLatin1String("layout://")))
        normNovName = QLatin1String("layout://") + normNovName;
    return normNovName + QLatin1String("?") + normItemName;
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

/********* Cell spacing property **********/
QSizeF QnLayoutResource::cellSpacing() const {
    QMutexLocker locker(&m_mutex);
    return m_cellSpacing;
}

void QnLayoutResource::setCellSpacing(const QSizeF &cellSpacing) {
    {
        QMutexLocker locker(&m_mutex);
        if(qFuzzyCompare(m_cellSpacing, cellSpacing))
            return;
        m_cellSpacing = cellSpacing;
    }
    emit cellSpacingChanged(::toSharedPointer(this));
}

void QnLayoutResource::setCellSpacing(qreal horizontalSpacing, qreal verticalSpacing) {
    setCellSpacing(QSizeF(horizontalSpacing, verticalSpacing));
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

/********* Background image property **********/
int QnLayoutResource::backgroundImageId() const {
    QMutexLocker locker(&m_mutex);
    return m_backgroundImageId;
}

void QnLayoutResource::setBackgroundImageId(int id) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_backgroundImageId == id)
            return;
        m_backgroundImageId = id;
    }
    emit backgroundImageChanged(::toSharedPointer(this));
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

