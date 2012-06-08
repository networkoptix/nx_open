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
        ItemHandlesMovement = 0x1,
        ItemHandlesResizing = 0x2
    };
    Q_DECLARE_FLAGS(HandlingFlags, HandlingFlag);

    static const GraphicsItemChange ItemHandlingFlagsChange = static_cast<GraphicsItemChange>(0x1000);
    static const GraphicsItemChange ItemHandlingFlagsHaveChanged = static_cast<GraphicsItemChange>(0x1001);

    /* Get basic syntax highlighting. */
#define ItemHandlingFlagsChange ItemHandlingFlagsChange
#define ItemHandlingFlagsHaveChanged ItemHandlingFlagsHaveChanged

    GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);
    virtual ~GraphicsWidget();

    HandlingFlags handlingFlags() const;
    void setHandlingFlags(HandlingFlags handlingFlags);
    void setHandlingFlag(HandlingFlag flag, bool value);

    GraphicsStyle *style() const;
    void setStyle(GraphicsStyle *style);
    using base_type::setStyle;

protected:
    GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    virtual void changeEvent(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    virtual void initStyleOption(QStyleOption *option) const;

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GraphicsWidget::HandlingFlags);

#endif // QN_GRAPHICS_WIDGET_H
