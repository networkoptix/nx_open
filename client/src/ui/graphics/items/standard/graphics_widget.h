#ifndef QN_GRAPHICS_WIDGET_H
#define QN_GRAPHICS_WIDGET_H

#include <QtGui/QGraphicsWidget>
#include "graphics_style.h"

class GraphicsWidgetPrivate;

class GraphicsWidget: public QGraphicsWidget {
    Q_OBJECT;
    
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
         * process all pending layout requests.
         * 
         * Note that this flag is considered set if
         * <tt>QGraphicsLayout::instantInvalidatePropagation()</tt> is <tt>true</tt>.
         *
         * This flag is not set by default.
         */
        ItemHandlesLayoutRequests = 0x4
    };
    Q_DECLARE_FLAGS(HandlingFlags, HandlingFlag);

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

    GraphicsStyle *style() const;
    void setStyle(GraphicsStyle *style);
    using base_type::setStyle;

    /**
     * \returns                         The area inside the widget's margins.
     */
    QRectF contentsRect() const;

    /**
     * Processes pending layout requests for all graphics widgets on the given
     * scene that have <tt>ItemHandlesLayoutRequests</tt> flag unset.
     *
     * \param scene                     Graphics scene to process items at.
     */
    static void handlePendingLayoutRequests(QGraphicsScene *scene);

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

    virtual void updateGeometry() override;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    virtual void changeEvent(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

    virtual void initStyleOption(QStyleOption *option) const;

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF& pos) const override;

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GraphicsWidget::HandlingFlags);

#endif // QN_GRAPHICS_WIDGET_H
