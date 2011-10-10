#ifndef GRAPHICSWIDGET_H
#define GRAPHICSWIDGET_H

#include <QGraphicsWidget>
#include <QScopedPointer>

/* A quick workaround. Remove this once we have a better solution. */
#undef override
#define override

class GraphicsWidgetPrivate;

class GraphicsWidget: public QGraphicsWidget
{
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent item for this graphics widget.
     */
    GraphicsWidget(QGraphicsItem *parent = NULL);

    enum GraphicsExtraFlag {
        ItemIsResizable = 0x1,          /**< Whether this item is resizable. */
    };
    Q_DECLARE_FLAGS(GraphicsExtraFlags, GraphicsExtraFlag);

    static const GraphicsItemChange ItemExtraFlagsChange = static_cast<GraphicsItemChange>(128);
    static const GraphicsItemChange ItemExtraFlagsHaveChanged = static_cast<GraphicsItemChange>(129);

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

    /**
     * Virtual destructor.
     */
    virtual ~GraphicsWidget();

signals:
    void clicked();
    void doubleClicked();
    void resizingStarted();
    void resizingFinished();
    void draggingStarted();
    void draggingFinished();

protected:
    GraphicsWidget(QGraphicsItem *parent, GraphicsWidgetPrivate *dd);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GraphicsWidget::GraphicsExtraFlags);

#endif // GRAPHICSWIDGET_H
