#ifndef GRAPHICSWIDGET_H
#define GRAPHICSWIDGET_H

#include <QtCore/QScopedPointer>

#include <QtGui/QGraphicsWidget>

#include <ui/graphics/instruments/instrumented.h>

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
     * \param wFlags                    Window flags for this graphics widget.
     */
    GraphicsWidget(QGraphicsItem *parent = 0, Qt::WindowFlags wFlags = 0);

    /**
     * Virtual destructor.
     */
    virtual ~GraphicsWidget();

    static const GraphicsItemChange ItemSizeChange = static_cast<GraphicsItemChange>(ItemPositionChange + 127);
    static const GraphicsItemChange ItemSizeHasChanged = static_cast<GraphicsItemChange>(ItemPositionChange + 128);

protected:
    virtual void initStyleOption(QStyleOption *option) const override;
    virtual bool event(QEvent *event) override;

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags wFlags = 0);

    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget)
    Q_DISABLE_COPY(GraphicsWidget)
};

#endif // GRAPHICSWIDGET_H
