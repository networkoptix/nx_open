#ifndef QN_GRAPHICS_WIDGET_P_H
#define QN_GRAPHICS_WIDGET_P_H

#include "graphics_widget.h"

class GraphicsStyle;
class GraphicsWidgetSceneData;

class GraphicsWidgetPrivate {
public:
    GraphicsWidgetPrivate(): q_ptr(NULL), handlingFlags(0), style(NULL) {};
    virtual ~GraphicsWidgetPrivate() {}

    GraphicsWidgetSceneData *ensureSceneData();
    
    static bool movableAncestorIsSelected(const QGraphicsItem *item);

protected:
    GraphicsWidget *q_ptr;
    GraphicsWidget::HandlingFlags handlingFlags;
    QWeakPointer<GraphicsWidgetSceneData> sceneData;
    mutable GraphicsStyle *style;
    mutable QScopedPointer<GraphicsStyle> reserveStyle;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};


#endif // QN_GRAPHICS_WIDGET_P_H
