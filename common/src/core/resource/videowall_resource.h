#pragma once

#include <QtCore/QRectF>
#include <nx/utils/uuid.h>

#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>

#include <utils/common/threadsafe_item_storage.h>

class QnVideoWallResource: public QnResource,
    private QnThreadsafeItemStorageNotifier<QnVideoWallItem>,
    private QnThreadsafeItemStorageNotifier<QnVideoWallPcData>,
    private QnThreadsafeItemStorageNotifier<QnVideoWallMatrix>
{
    Q_OBJECT
    typedef QnResource base_type;

public:
    QnVideoWallResource(QnCommonModule* commonModule = nullptr);

    QnThreadsafeItemStorage<QnVideoWallItem> *items() const;
    QnThreadsafeItemStorage<QnVideoWallPcData> *pcs() const;
    QnThreadsafeItemStorage<QnVideoWallMatrix> *matrices() const;

    /** \returns Whether the videowall should be started when the PC boots up. */
    bool isAutorun() const;
    void setAutorun(bool value);

    /** Utility method to get IDs of all online items.  */
    QList<QnUuid> onlineItems() const;

    virtual Qn::ResourceStatus getStatus() const override;

signals:
    void itemAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);
    void itemRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);
    void itemChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallItem& item,
        const QnVideoWallItem& oldItem);

    void pcAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);
    void pcRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);
    void pcChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallPcData &pc);

    void matrixAdded(const QnVideoWallResourcePtr &resource, const QnVideoWallMatrix &matrix);
    void matrixRemoved(const QnVideoWallResourcePtr &resource, const QnVideoWallMatrix &matrix);
    void matrixChanged(const QnVideoWallResourcePtr &resource, const QnVideoWallMatrix &matrix);

    void autorunChanged(const QnResourcePtr &resource);

protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

    virtual Qn::Notifier storedItemAdded(const QnVideoWallItem& item) override;
    virtual Qn::Notifier storedItemRemoved(const QnVideoWallItem& item) override;
    virtual Qn::Notifier storedItemChanged(const QnVideoWallItem& item,
        const QnVideoWallItem& oldItem) override;

    virtual Qn::Notifier storedItemAdded(const QnVideoWallPcData& item) override;
    virtual Qn::Notifier storedItemRemoved(const QnVideoWallPcData& item) override;
    virtual Qn::Notifier storedItemChanged(const QnVideoWallPcData& item) override;

    virtual Qn::Notifier storedItemAdded(const QnVideoWallMatrix& item) override;
    virtual Qn::Notifier storedItemRemoved(const QnVideoWallMatrix& item) override;
    virtual Qn::Notifier storedItemChanged(const QnVideoWallMatrix& item) override;
private:
    bool m_autorun;

    QScopedPointer<QnThreadsafeItemStorage<QnVideoWallItem> > m_items;
    QScopedPointer<QnThreadsafeItemStorage<QnVideoWallPcData> > m_pcs;
    QScopedPointer<QnThreadsafeItemStorage<QnVideoWallMatrix> > m_matrices;
};

Q_DECLARE_METATYPE(QnVideoWallResourcePtr)
Q_DECLARE_METATYPE(QnVideoWallResourceList)
