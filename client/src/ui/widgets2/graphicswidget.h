#ifndef GRAPHICSWIDGET_H
#define GRAPHICSWIDGET_H

#include <QtCore/QScopedPointer>

#include <QtGui/QGraphicsWidget>

#include <ui/instruments/instrumented.h>

/* A quick workaround. Remove this once we have a better solution. */
#undef override
#define override

class GraphicsWidgetPrivate;

class GraphicsWidget: public Instrumented<QGraphicsWidget>
{
    Q_OBJECT;

    typedef Instrumented<QGraphicsWidget> base_type;

public:
    /**
     * Constructor.
     *
     * \param parent                    Parent item for this graphics widget.
     */
    GraphicsWidget(QGraphicsItem *parent = 0, Qt::WindowFlags wFlags = 0);

    /**
     * Virtual destructor.
     */
    virtual ~GraphicsWidget();

    enum GraphicsExtraFlag {
        ItemIsResizable = 0x1,          /**< Whether this item is resizable. */
        ItemIsDraggable = 0x2,          /**< Whether this item is draggable. */
    };
    Q_DECLARE_FLAGS(GraphicsExtraFlags, GraphicsExtraFlag)

    static const GraphicsItemChange ItemSizeChange = static_cast<GraphicsItemChange>(ItemPositionChange + 127);
    static const GraphicsItemChange ItemSizeHasChanged = static_cast<GraphicsItemChange>(ItemPositionChange + 128);
    static const GraphicsItemChange ItemExtraFlagsChange = static_cast<GraphicsItemChange>(ItemPositionChange + 129);
    static const GraphicsItemChange ItemExtraFlagsHasChanged = static_cast<GraphicsItemChange>(ItemPositionChange + 130);

    /**
     * \returns                         Flags for this widget.
     */
    GraphicsExtraFlags extraFlags() const;

    /**
     * \param flag                      Widget flag to change.
     * \param enabled                   Whether to set or unset the given flag.
     */
    void setExtraFlag(GraphicsExtraFlag flag, bool enabled = true);

    /**
     * \param flags                     New widget flags.
     */
    void setExtraFlags(GraphicsExtraFlags flags);

Q_SIGNALS:
    void clicked();
    void doubleClicked();
    void resizingStarted();
    void resizingFinished();
    void movingStarted();
    void movingFinished();

protected:
    void initStyleOption(QStyleOption *option) const;

    bool event(QEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    bool windowFrameEvent(QEvent *event) override;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;

protected:
    Qt::WindowFrameSection filterWindowFrameSection(Qt::WindowFrameSection section) const;

    virtual QDrag *createDrag(QGraphicsSceneMouseEvent *event);
    virtual Qt::DropAction startDrag(QDrag *drag);
    virtual void endDrag(Qt::DropAction dropAction);

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags wFlags = 0);

    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    void drag(QGraphicsSceneMouseEvent *event); // ### move to GraphicsWidgetPrivate?

private:
    Q_DECLARE_PRIVATE(GraphicsWidget)
    Q_DISABLE_COPY(GraphicsWidget)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GraphicsWidget::GraphicsExtraFlags)

#endif // GRAPHICSWIDGET_H
