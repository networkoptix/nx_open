#include "layout_resource.h"
#include <utils/common/warnings.h>
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "api/serializer/pb_serializer.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "utils/common/common_meta_types.h"

QnLayoutResource::QnLayoutResource(): 
    base_type(),
    m_cellAspectRatio(-1.0),
    m_cellSpacing(-1.0, -1.0)
{
    QnCommonMetaTypes::initilize();

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

QnLayoutResourcePtr QnLayoutResource::fromFile(const QString& xfile)
{
    QnLayoutResourcePtr layout;
    QnLayoutFileStorageResource layoutStorage;
    layoutStorage.setUrl(xfile);
    QIODevice* layoutFile = layoutStorage.open(QLatin1String("layout.pb"), QIODevice::ReadOnly);
    if (layoutFile == 0)
        return layout;
    QByteArray layoutData = layoutFile->readAll();
    delete layoutFile;
    QnApiPbSerializer serializer;
    try {
        serializer.deserializeLayout(layout, layoutData);
        if (layout == 0)
            return layout;
    } catch(...) {
        return layout;
    }

    QIODevice* uuidFile = layoutStorage.open(QLatin1String("uuid.bin"), QIODevice::ReadOnly);
    if (uuidFile)
    {
        QByteArray data = uuidFile->readAll();
        delete uuidFile;
        layout->setGuid(QUuid(data.data()));
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceByGuid(layout->getGuid()).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            return existingLayout;
    }
    else {
        layout->setGuid(QUuid::createUuid());
    }

    QIODevice* rangeFile = layoutStorage.open(QLatin1String("range.bin"), QIODevice::ReadOnly);
    if (rangeFile)
    {
        QByteArray data = rangeFile->readAll();
        delete rangeFile;
        layout->setLocalRange(QnTimePeriod().deserialize(data));
    }

    layout->setParentId(0);
    layout->setId(QnId::generateSpecialId());
    layout->setName(QFileInfo(xfile).fileName());

    layout->addFlags(QnResource::url);
    layout->setUrl(xfile);

    QnLayoutItemDataMap items = layout->getItems();
    QnLayoutItemDataMap updatedItems;

    QIODevice* itemNamesIO = layoutStorage.open(QLatin1String("item_names.txt"), QIODevice::ReadOnly);
    QTextStream itemNames(itemNamesIO);

    // todo: here is bad place to add resources to pool. need rafactor
    for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        QnLayoutItemData& item = itr.value();
        QString path = item.resource.path;
        item.uuid = QUuid::createUuid();
        item.resource.id = QnId::generateSpecialId();
        if (!path.endsWith(QLatin1String(".mkv")))
            item.resource.path += QLatin1String(".mkv");
        item.resource.path = updateNovParent(xfile,item.resource.path);
        updatedItems.insert(item.uuid, item);

        QnStorageResourcePtr storage(new QnLayoutFileStorageResource());
        storage->setUrl(xfile);

        QnAviResourcePtr aviResource(new QnAviResource(item.resource.path));
        aviResource->setStorage(storage);
        aviResource->setId(item.resource.id);
        aviResource->removeFlags(QnResource::local); // do not display in tree root and disable 'open in container folder' menu item
        QString itemName(itemNames.readLine());
        if (!itemName.isEmpty())
            aviResource->setName(itemName);

        int numberOfChannels = aviResource->getVideoLayout()->numberOfChannels();
        for (int channel = 0; channel < numberOfChannels; ++channel)
        {
            QIODevice* motionIO = layoutStorage.open(QString(QLatin1String("motion%1_%2.bin")).arg(channel).arg(QFileInfo(path).baseName()), QIODevice::ReadOnly);
            if (motionIO) {
                Q_ASSERT(motionIO->size() % sizeof(QnMetaDataV1Light) == 0);
                QnMetaDataLightVector motionData;
                int motionDataSize = motionIO->size() / sizeof(QnMetaDataV1Light);
                if (motionDataSize > 0) {
                    motionData.resize(motionDataSize);
                    motionIO->read((char*) &motionData[0], motionIO->size());
                }
                delete motionIO;
                for (uint i = 0; i < motionData.size(); ++i)
                    motionData[i].doMarshalling();
                if (!motionData.empty())
                    aviResource->setMotionBuffer(motionData, channel);
            }
        }

        qnResPool->addResource(aviResource);
    }
    delete itemNamesIO;
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
