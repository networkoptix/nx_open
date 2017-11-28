#pragma once

#include <QtCore/QRect>

#include <QtGui/QTransform>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/screen_snap.h>

class QnVideowallManageWidget;

class QnVideowallManageWidgetPrivate: public QObject
{
    Q_OBJECT

public:
    QnVideowallManageWidgetPrivate(QnVideowallManageWidget* q);

    QTransform getTransform(const QRect &rect);
    QTransform getInvertedTransform(const QRect &rect);
    QRect targetRect(const QRect &rect) const;

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall);

    void paint(QPainter* painter, const QRect &rect);

    void mouseEnter();
    void mouseLeave();
    void mouseMoveAt(const QPoint &pos);
    void mouseClickAt(const QPoint &pos, Qt::MouseButtons buttons);
    void dragStartAt(const QPoint &pos);
    void dragMoveAt(const QPoint &pos, const QPoint &oldPos);
    void dragEndAt(const QPoint &pos);

    void tick(int deltaMSecs);

    /** Count of videowall items that is proposed to be on this PC. */
    int proposedItemsCount() const;

signals:
    void itemsChanged();

public:
    enum ItemTransformation {
        None            = 0x00,
        Move            = 0x01,
        ResizeLeft      = 0x02,
        ResizeRight     = 0x04,
        ResizeTop       = 0x08,
        ResizeBottom    = 0x10
    };
    Q_DECLARE_FLAGS(ItemTransformations, ItemTransformation)

private:
    enum class ItemType {
        Existing,
        ScreenPart,
        Screen,
        Added
    };

    enum StateFlag {
        Default = 0,        
        Pressed = 0x2,      
        Hovered = 0x4,
        DeleteHovered = 0x8      
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag)



    struct TransformationProcess {
        TransformationProcess();

        ItemTransformations value;
        QRect geometry;
        QRect oldGeometry;

        bool isRunning() const {
            return value != ItemTransformation::None;
        }

		void clear() {
			value = ItemTransformation::None;
			geometry = QRect();
			oldGeometry = QRect();
		}

        friend bool operator==(const TransformationProcess &process, const ItemTransformations &transformations) {
            return process.value == transformations;
        }

        friend bool operator==(const ItemTransformations &transformations, const TransformationProcess &process) {
            return process.value == transformations;
        }
    };

    struct BaseModelItem {
        BaseModelItem(const QRect &geometry, ItemType itemType, const QnUuid &id, QnVideowallManageWidget* q);
        virtual ~BaseModelItem();

        virtual bool free() const = 0;
        virtual void setFree(bool value) = 0;

        virtual void paint(QPainter* painter, const TransformationProcess &process) const;
        virtual QRect bodyRect() const;
        virtual QRect deleteButtonRect() const;
        virtual QPainterPath bodyPath() const;
        virtual int fontSize() const;
        virtual int iconSize() const;
        virtual QColor baseColor() const = 0;
        virtual void paintProposed(QPainter* painter, const QRect &proposedGeometry) const;
        void paintDashBorder(QPainter *painter, const QPainterPath &path) const;
        void paintResizeAnchors(QPainter *painter, const QRect &rect) const;
        void paintPixmap(QPainter *painter, const QRect &rect, const QPixmap &pixmap) const;
        void paintDeleteButton(QPainter *painter) const;

        bool hasFlag(StateFlags flag) const;
        bool isPartOfScreen() const;

        QnUuid id;
        ItemType itemType;

        QRect geometry;
        QnScreenSnaps snaps;
        StateFlags flags;

        qreal opacity;
        qreal deleteButtonOpacity;
        bool editable;

        QString name;
    protected:
        Q_DECLARE_PUBLIC(QnVideowallManageWidget)
        QnVideowallManageWidget *q_ptr;
    };


    struct ModelItem: BaseModelItem {
        typedef BaseModelItem base_type;

        ModelItem(ItemType itemType, const QnUuid &id, QnVideowallManageWidget* q);

        virtual bool free() const override;
        virtual void setFree(bool value) override;
        virtual QColor baseColor() const override;

        virtual void paintProposed(QPainter* painter, const QRect &proposedGeometry) const override;
    };

    struct FreeSpaceItem: BaseModelItem {
        typedef BaseModelItem base_type;

        FreeSpaceItem(const QRect &rect, ItemType itemType, QnVideowallManageWidget* q);
        virtual QColor baseColor() const override;

        virtual void paint(QPainter* painter, const TransformationProcess &process) const override;
    };

    struct ModelScreenPart: FreeSpaceItem {
        typedef FreeSpaceItem base_type;

        ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect, QnVideowallManageWidget* q);

        virtual bool free() const override;
        virtual void setFree(bool value) override;
    private:
        bool m_free;
    };

    struct ModelScreen: FreeSpaceItem {
        typedef FreeSpaceItem base_type;

        ModelScreen(int idx, const QRect &rect, QnVideowallManageWidget* q);

        virtual bool free() const override;
        virtual void setFree(bool value) override;

        virtual void paint(QPainter* painter, const TransformationProcess &process) const override;
        virtual void paintProposed(QPainter* painter, const QRect &proposedGeometry) const override;

        QList<ModelScreenPart> parts;
    };

private:
    Q_DECLARE_PUBLIC(QnVideowallManageWidget)

    /** 
     * Utility function that iterates over all items and calls handler to each of them. 
     * If the handler sets 'abort' variable to true, iterator will stop.
     */
    void foreachItem(std::function<void(BaseModelItem& /*item*/, bool& /*abort*/)> handler);
    void foreachItemConst(std::function<void(const BaseModelItem& /*item*/, bool& /*abort*/)> handler);

    void moveItem(BaseModelItem &item, const QPoint &offset);
    void resizeItem(BaseModelItem &item, const QPoint &offset);

    void setFree(const QnScreenSnaps &snaps, bool value);

    ItemTransformations transformationsAnchor(const QRect &geometry, const QPoint &pos) const;
    Qt::CursorShape transformationsCursor(ItemTransformations value) const;

	typedef std::function<QRect(const BaseModelItem &, bool *, bool *)> calculateProposedGeometryFunction;
    QRect calculateProposedResizeGeometry(const BaseModelItem &item, bool *valid = NULL, bool *multiScreen = NULL) const;
	QRect calculateProposedMoveGeometry(const BaseModelItem &item, bool *valid = NULL, bool *multiScreen = NULL) const;

	void processItemStart(BaseModelItem &item, calculateProposedGeometryFunction proposedGeometry);
	void processItemEnd(BaseModelItem &item, calculateProposedGeometryFunction proposedGeometry);
private:
    QnVideowallManageWidget *q_ptr;
    QTransform m_transform;
    QTransform m_invertedTransform;
    QRect m_widgetRect;
    QRect m_unitedGeometry;
    QList<ModelItem> m_items;
    QList<ModelScreen> m_screens;

    TransformationProcess m_process;   
};

/* 
 * If we allow this function in windows, 'case expression not constant' will be raised. MSVS 2012 does not support constexpr yet.
 * Without this function we have an error: invalid conversion from int to QnVideowallManageWidgetPrivate::ItemTransformation [-fpermissive] in GCC
 */
#ifdef __GNUC__
    Q_DECLARE_OPERATORS_FOR_FLAGS(QnVideowallManageWidgetPrivate::ItemTransformations)
#endif
