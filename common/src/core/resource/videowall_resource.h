#ifndef VIDEOWALL_RESOURCE_H
#define VIDEOWALL_RESOURCE_H

#include <QtCore/QRectF>
#include <QtCore/QUuid>

#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>

class QnVideoWallResource : public QnResource
{
    Q_OBJECT
    typedef QnResource base_type;

public:
    QnVideoWallResource();

    virtual QString getUniqueId() const override;

    void setItems(const QnVideoWallItemList &items);
    void setItems(const QnVideoWallItemMap &items);
    QnVideoWallItemMap getItems() const;
    QnVideoWallItem getItem(const QUuid &itemUuid) const;
    bool hasItem(const QUuid &itemUuid) const;
    void addItem(const QnVideoWallItem &item);
    void removeItem(const QnVideoWallItem &item);
    void removeItem(const QUuid &itemUuid);
    void updateItem(const QUuid &itemUuid, const QnVideoWallItem &item);

    void setPcs(const QnVideoWallPcDataList &pcs);
    void setPcs(const QnVideoWallPcDataMap &pcs);
    QnVideoWallPcDataMap getPcs() const;
    QnVideoWallPcData getPc(const QUuid &pcUuid) const;
    bool hasPc(const QUuid &pcUuid) const;
    void addPc(const QnVideoWallPcData &pc);
    void removePc(const QnVideoWallPcData &pc);
    void removePc(const QUuid &pcUuid);
    void updatePc(const QUuid &pcUuid, const QnVideoWallPcData &pc);

signals:
    void itemAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);
    void itemRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);
    void itemChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);

    void pcAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);
    void pcRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);
    void pcChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    void addItemUnderLock(const QnVideoWallItem &item);
    void updateItemUnderLock(const QUuid &itemUuid, const QnVideoWallItem &item);
    void removeItemUnderLock(const QUuid &itemUuid);

    void addPcUnderLock(const QnVideoWallPcData &pc);
    void updatePcUnderLock(const QUuid &pcUuid, const QnVideoWallPcData &pc);
    void removePcUnderLock(const QUuid &pcUuid);

private:
    QnVideoWallItemMap m_itemByUuid;
    QnVideoWallPcDataMap m_pcByUuid;
};

Q_DECLARE_METATYPE(QnVideoWallResourcePtr)
Q_DECLARE_METATYPE(QnVideoWallResourceList)

#endif // VIDEOWALL_RESOURCE_H
