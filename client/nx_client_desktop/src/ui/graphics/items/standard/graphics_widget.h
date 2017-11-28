#ifndef QN_GRAPHICS_WIDGET_H
#define QN_GRAPHICS_WIDGET_H

#include <QtWidgets/QGraphicsWidget>

#include <ui/common/frame_section_queryable.h>

#include "graphics_style.h"

class GraphicsWidgetPrivate;

class GraphicsWidget: public QGraphicsWidget, public FrameSectionQueryable
{
    Q_OBJECT

    typedef QGraphicsWidget base_type;

public:
    enum HandlingFlag {
        /**
         * Item's event handlers provide default implementation of item
         * movement with left mouse button. Note that this implementation
         * respects the <tt>QGraphicsItem::ItemIsMovable</tt> flag.
         *
         * This flag is not set by default.
         */
        ItemHandlesMovement = 0x1,

        /**
         * Item's event handlers provide default implementation of item
         * resizing with left mouse button for windows.
         * Note that window is a widget with <tt>Qt::Window</tt> and
         * <tt>Qt::WindowTitleHint</tt> flags set.
         *
         * This flag is not set by default.
         */
        ItemHandlesResizing = 0x2,

        /**
         * Item's layout changes are handled by default implementation in
         * <tt>QGraphicsWidget</tt>. If this flag is not set,
         * <tt>handlePendingLayoutRequests()</tt> function can be used to
         * force immediate processing of all pending layout requests,
         * e.g. before a paint event.
         *
         * Note that this flag has no effect if
         * <tt>QGraphicsLayout::instantInvalidatePropagation()</tt> is <tt>false</tt>.
         * In this case layout changes are always handled by default implementation.
         *
         * This flag is not set by default.
         */
        ItemHandlesLayoutRequests = 0x4
    };
    Q_DECLARE_FLAGS(HandlingFlags, HandlingFlag);

    enum TransformOrigin {
        Legacy,
        TopLeft,
        Top,
        TopRight,
        Left,
        Center,
        Right,
        BottomLeft,
        Bottom,
        BottomRight
    };

    /* Note that it is important for these values to fit into unsigned char as sizeof(GraphicsItemChange) may equal 1. */
    static const GraphicsItemChange ItemHandlingFlagsChange = static_cast<GraphicsItemChange>(0x80u);
    static const GraphicsItemChange ItemHandlingFlagsHaveChanged = static_cast<GraphicsItemChange>(0x81u);

    /* Get basic syntax highlighting. */
#define ItemHandlingFlagsChange ItemHandlingFlagsChange
#define ItemHandlingFlagsHaveChanged ItemHandlingFlagsHaveChanged

    GraphicsWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~GraphicsWidget();

    HandlingFlags handlingFlags() const;
    void setHandlingFlags(HandlingFlags handlingFlags);
    void setHandlingFlag(HandlingFlag flag, bool value);

    TransformOrigin transformOrigin() const;
    void setTransformOrigin(TransformOrigin transformOrigin);

    qreal resizeEffectRadius() const;
    void setResizeEffectRadius(qreal resizeEffectRadius);

    QMarginsF contentsMargins() const;
    using base_type::setContentsMargins;
    void setContentsMargins(const QMarginsF &margins);

    /**
     * \returns                         The area inside the widget's margins.
     */
    QRectF contentsRect() const;

    virtual void setGeometry(const QRectF &rect) override;

    /**
     * Processes pending layout requests for all graphics widgets on the given
     * scene that have <tt>ItemHandlesLayoutRequests</tt> flag unset.
     *
     * \param scene                     Graphics scene to process items at.
     */
    static void handlePendingLayoutRequests(QGraphicsScene *scene);

    void selectThisWidget(bool clearOtherSelection = true);

signals:
    void transformOriginChanged();

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

    virtual void updateGeometry() override;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    virtual bool event(QEvent *event) override;
    virtual void polishEvent() override;
    virtual void changeEvent(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

    virtual void initStyleOption(QStyleOption *option) const;

    using FrameSectionQueryable::windowFrameSectionAt;
    using FrameSectionQueryable::windowCursorAt;
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF& pos) const override;
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override;

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GraphicsWidget::HandlingFlags);

#endif // QN_GRAPHICS_WIDGET_H
