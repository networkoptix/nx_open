#include "layout_resource.h"
#include <utils/common/warnings.h>
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "api/serializer/pb_serializer.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resource_managment/resource_pool.h"

QnLayoutResource::QnLayoutResource(): 
    base_type(),
    m_cellAspectRatio(-1.0),
    m_cellSpacing(-1.0, -1.0)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QnLayoutItemData>();
        metaTypesInitialized = true;
    }
    
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

    emit cellAspectRatioChanged();
}

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

    emit cellSpacingChanged();
}

void QnLayoutResource::setCellSpacing(qreal horizontalSpacing, qreal verticalSpacing) {
    setCellSpacing(QSizeF(horizontalSpacing, verticalSpacing));
}

void QnLayoutResource::addItemUnderLock(const QnLayoutItemData &item) {
    if(m_itemByUuid.contains(item.uuid)) {
        qnWarning("Item with UUID %1 is already in this layout resource.", item.uuid.toString());
        return;
    }

    m_itemByUuid[item.uuid] = item;

    m_mutex.unlock();
    emit itemAdded(item);
    m_mutex.lock();
}

void QnLayoutResource::updateItemUnderLock(const QUuid &itemUuid, const QnLayoutItemData &item) {
    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end()) {
        qnWarning("There is no item with UUID %1 in this layout.", itemUuid.toString());
        return;
    }

    if(*pos == item) {
        pos->dataByRole = item.dataByRole; // TODO: hack hack hack
        return;
    }

    *pos = item;

    m_mutex.unlock();
    emit itemChanged(item);
    m_mutex.lock();
}

void QnLayoutResource::removeItemUnderLock(const QUuid &itemUuid) {
    QnLayoutItemDataMap::iterator pos = m_itemByUuid.find(itemUuid);
    if(pos == m_itemByUuid.end())
        return;

    QnLayoutItemData item = *pos;
    m_itemByUuid.erase(pos);

    m_mutex.lock();
    emit itemRemoved(item);
    m_mutex.unlock();
}

QnLayoutResourcePtr QnLayoutResource::fromFile(const QString& xfile)
{
    QnLayoutResourcePtr layout;
    QnLayoutFileStorageResource layoutStorage;
    layoutStorage.setUrl(xfile);
    QIODevice* layoutFile = layoutStorage.open(QLatin1String("layout.pb"), QIODevice::ReadOnly);
    if (layoutFile == 0)
        return layout;
    QByteArray layoutData = layoutFile->readAll();

    QnApiPbSerializer serializer;
    try {
        serializer.deserializeLayout(layout, layoutData);
        if (layout == 0)
            return layout;
    } catch(...) {
        return layout;
    }
    layout->setGuid(QUuid::createUuid());
    layout->setParentId(0);
    layout->setId(QnId::generateSpecialId());
    layout->setName(QFileInfo(xfile).fileName() + QLatin1String(" - ") + layout->getName());

    layout->addFlags(QnResource::url);
    layout->setUrl(xfile);

    QnLayoutItemDataMap items = layout->getItems();
    QnLayoutItemDataMap updatedItems;

    // todo: here is bad place to add resources to pool. need rafactor
    for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        QnLayoutItemData& item = itr.value();
        QString path = item.resource.path;
        item.uuid = QUuid::createUuid();
        item.resource.id = QnId::generateSpecialId();
        item.resource.path = QLatin1String("layout://") + xfile + QLatin1Char('?') + path + QLatin1String(".mkv");
        updatedItems.insert(item.uuid, item);

        QnStorageResourcePtr storage(new QnLayoutFileStorageResource());
        storage->setUrl(QLatin1String("layout://") + xfile);

        QnAviResourcePtr aviResource(new QnAviResource(item.resource.path));
        aviResource->setStorage(storage);
        aviResource->setId(item.resource.id);
        aviResource->removeFlags(QnResource::local); // do not display in tree root and disable 'open in container folder' menu item


        QIODevice* motionIO = layoutStorage.open(QString(QLatin1String("motion_%1.bin")).arg(path), QIODevice::ReadOnly);
        if (motionIO) {
            Q_ASSERT(motionIO->size() % sizeof(QnMetaDataV1Light) == 0);
            QVector<QnMetaDataV1Light> motionData;
            int motionDataSize = motionIO->size() / sizeof(QnMetaDataV1Light);
            if (motionDataSize > 0) {
                motionData.resize(motionDataSize);
                motionIO->read((char*) &motionData[0], motionIO->size());
            }
            delete motionIO;
            for (int i = 0; i < motionData.size(); ++i)
                motionData[i].doMarshalling();
            aviResource->setMotionBuffer(motionData);
        }


        qnResPool->addResource(aviResource);
    }
    layout->setItems(updatedItems);
    layout->addFlags(QnResource::local_media);
    return layout;
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
