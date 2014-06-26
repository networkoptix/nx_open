#ifndef VIDEOWALL_MANAGE_WIDGET_PRIVATE_H
#define VIDEOWALL_MANAGE_WIDGET_PRIVATE_H

#include <QtCore/QRect>

#include <QtGui/QTransform>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/screen_snap.h>

#include <ui/common/geometry.h>

class QnVideowallManageWidgetPrivate : private QnGeometry {
    Q_DECLARE_TR_FUNCTIONS(QnVideowallManageWidgetPrivate);
public:
    QnVideowallManageWidgetPrivate();

    QTransform getTransform(const QRect &rect);
    QTransform getInvertedTransform(const QRect &rect);
    QRect targetRect(const QRect &rect) const;

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall);

    void paint(QPainter* painter, const QRect &rect);

    void mouseMoveAt(const QPoint &pos, Qt::MouseButtons buttons);
    void mouseClickAt(const QPoint &pos, Qt::MouseButtons buttons);
    void dragStartAt(const QPoint &pos);
    void dragMoveAt(const QPoint &pos);
    void dragEndAt(const QPoint &pos);

    void tick(int deltaMSecs);
private:
    enum class ItemType {
        Existing,
        ScreenPart,
        Screen,
        Added
    };

    enum class StateFlag {
        Default = 0,        
        Pressed = 0x2,      
        Hovered = 0x4,      
        Disabled = 0x8      
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag)

    struct BaseModelItem {
        BaseModelItem(const QRect &rect, ItemType itemType, const QUuid &id);

        virtual bool free() const = 0;
        virtual void setFree(bool value) = 0;

        virtual void paint(QPainter* painter) const;

        const QUuid id;
        const ItemType itemType;

        QRect geometry;
        QnScreenSnaps snaps;
        StateFlags flags;

        qreal opacity;
        QColor baseColor;
    };


    struct ModelItem: BaseModelItem {
        ModelItem(ItemType itemType, const QUuid &id);

        virtual bool free() const override;
        virtual void setFree(bool value) override;
    };

    struct ExistingItem: ModelItem {
        ExistingItem(const QUuid &id);
    };

    struct AddedItem: ModelItem {
        AddedItem();
    };

    struct FreeSpaceItem: BaseModelItem {
        typedef BaseModelItem base_type;

        FreeSpaceItem(const QRect &rect, ItemType itemType);

        virtual void paint(QPainter* painter) const override;
    };

    struct ModelScreenPart: FreeSpaceItem {
        typedef FreeSpaceItem base_type;

        ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect);

        virtual bool free() const override;
        virtual void setFree(bool value) override;
    private:
        bool m_free;
    };

    struct ModelScreen: FreeSpaceItem {
        typedef FreeSpaceItem base_type;

        ModelScreen(int idx, const QRect &rect);

        virtual bool free() const override;
        virtual void setFree(bool value) override;

        virtual void paint(QPainter* painter) const override;

        QList<ModelScreenPart> parts;
    };

private:
    /** 
     * Utility function that iterates over all items and calls handler to each of them. 
     * If the handler sets 'abort' variable to true, iterator will stop.
     */
    void foreachItem(std::function<void(BaseModelItem& /*item*/, bool& /*abort*/)> handler);
    void foreachItemConst(std::function<void(const BaseModelItem& /*item*/, bool& /*abort*/)> handler);

    void use(const QnScreenSnaps &snaps);
private:
    QTransform m_transform;
    QTransform m_invertedTransform;
    QRect m_widgetRect;
    QRect m_unitedGeometry;
    QList<ModelItem> m_items;
    QList<ModelScreen> m_screens;
    QList<ModelItem> m_added;
};

#endif // VIDEOWALL_MANAGE_WIDGET_PRIVATE_H
