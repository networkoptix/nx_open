#ifndef VIDEOWALL_RESOURCE_H
#define VIDEOWALL_RESOURCE_H

#include <QtCore/QRectF>
#include <utils/common/uuid.h>

#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>
#include <utils/common/threadsafe_item_storage.h>

class QnVideoWallResource : public QnResource, 
    private QnThreadsafeItemStorageNotifier<QnVideoWallItem>,
    private QnThreadsafeItemStorageNotifier<QnVideoWallPcData>,
    private QnThreadsafeItemStorageNotifier<QnVideoWallMatrix>
{
    Q_OBJECT
    typedef QnResource base_type;

public:
    QnVideoWallResource();

    QnThreadsafeItemStorage<QnVideoWallItem> *items() const;
    QnThreadsafeItemStorage<QnVideoWallPcData> *pcs() const;
    QnThreadsafeItemStorage<QnVideoWallMatrix> *matrices() const;

    /** \returns Whether the videowall should be started when the PC boots up. */
    bool isAutorun() const;
    void setAutorun(bool value);

    /** Utility method to get IDs of all online items.  */
    QList<QnUuid> onlineItems() const;
signals:
    void itemAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);
    void itemRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);
    void itemChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);

    void pcAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);
    void pcRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);
    void pcChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);

    void matrixAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallMatrix &matrix);
    void matrixRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallMatrix &matrix);
    void matrixChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallMatrix &matrix);

    void autorunChanged(const QnResourcePtr &resource);
protected:
    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    virtual void storedItemAdded(const QnVideoWallItem &item) override;
    virtual void storedItemRemoved(const QnVideoWallItem &item) override;
    virtual void storedItemChanged(const QnVideoWallItem &item) override;

    virtual void storedItemAdded(const QnVideoWallPcData &item) override;
    virtual void storedItemRemoved(const QnVideoWallPcData &item) override;
    virtual void storedItemChanged(const QnVideoWallPcData &item) override;

    virtual void storedItemAdded(const QnVideoWallMatrix &item) override;
    virtual void storedItemRemoved(const QnVideoWallMatrix &item) override;
    virtual void storedItemChanged(const QnVideoWallMatrix &item) override;
private:
    bool m_autorun;

    QScopedPointer<QnThreadsafeItemStorage<QnVideoWallItem> > m_items;
    QScopedPointer<QnThreadsafeItemStorage<QnVideoWallPcData> > m_pcs;
    QScopedPointer<QnThreadsafeItemStorage<QnVideoWallMatrix> > m_matrices;
};

Q_DECLARE_METATYPE(QnVideoWallResourcePtr)
Q_DECLARE_METATYPE(QnVideoWallResourceList)

#endif // VIDEOWALL_RESOURCE_H
