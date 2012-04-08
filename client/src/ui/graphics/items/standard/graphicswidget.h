#ifndef QN_GRAPHICS_WIDGET_H
#define QN_GRAPHICS_WIDGET_H

#include <QtGui/QGraphicsWidget>

class GraphicsWidgetPrivate;

class GraphicsWidget: public QGraphicsWidget {
public:
    GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

    virtual ~GraphicsWidget();

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};


#endif // QN_GRAPHICS_WIDGET_H
