#ifndef QN_GRAPHICS_WIDGET_H
#define QN_GRAPHICS_WIDGET_H

#include <QtGui/QGraphicsWidget>
#include "graphics_style.h"

class GraphicsWidgetPrivate;

class GraphicsWidget: public QGraphicsWidget {
    Q_OBJECT;
    
    typedef QGraphicsWidget base_type;

public:
    GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

    virtual ~GraphicsWidget();

    GraphicsStyle *style() const;
    void setStyle(GraphicsStyle *style);
    using base_type::setStyle;

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

    virtual void changeEvent(QEvent *event) override;
    
    virtual void initStyleOption(QStyleOption *option) const;

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};


#endif // QN_GRAPHICS_WIDGET_H
