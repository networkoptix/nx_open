#ifndef VIDEOWALL_RESOURCE_H
#define VIDEOWALL_RESOURCE_H

#include <QtCore/QRectF>
#include <QtCore/QUuid>

#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/resource_item_storage.h>

class QnVideoWallResource : public QnResource, 
    private QnResourceItemStorageNotifier<QnVideoWallItem>,
    private QnResourceItemStorageNotifier<QnVideoWallPcData>,
    private QnResourceItemStorageNotifier<QnVideoWallMatrix>
{
    Q_OBJECT
    typedef QnResource base_type;

public:
    QnVideoWallResource();

    QnResourceItemStorage<QnVideoWallItem> *items() const;
    QnResourceItemStorage<QnVideoWallPcData> *pcs() const;
    QnResourceItemStorage<QnVideoWallMatrix> *matrices() const;

    /** \returns Whether the videowall should be started when the PC boots up. */
    bool isAutorun() const;
    void setAutorun(bool value);

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
   
    QScopedPointer<QnResourceItemStorage<QnVideoWallItem> > m_items;
    QScopedPointer<QnResourceItemStorage<QnVideoWallPcData> > m_pcs;
    QScopedPointer<QnResourceItemStorage<QnVideoWallMatrix> > m_matrices;

    bool m_autorun;
};

Q_DECLARE_METATYPE(QnVideoWallResourcePtr)
Q_DECLARE_METATYPE(QnVideoWallResourceList)

#endif // VIDEOWALL_RESOURCE_H
