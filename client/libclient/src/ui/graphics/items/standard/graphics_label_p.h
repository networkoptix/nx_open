#ifndef GRAPHICSLABEL_P_H
#define GRAPHICSLABEL_P_H

#include "graphics_frame_p.h"
#include "graphics_label.h"

class QGraphicsSimpleTextItem;

class GraphicsLabelPrivate : public GraphicsFramePrivate
{
    Q_DECLARE_PUBLIC(GraphicsLabel)

public:
    void init();

    void updateSizeHint();
    
    void ensurePixmaps();
    void ensureStaticText();

    QColor textColor() const;

    GraphicsLabel::PerformanceHint performanceHint;
    Qt::Alignment alignment;
    QString text;
    QStaticText staticText;
    QPixmap pixmap;
    QRectF rect;
    bool staticTextDirty;
    bool pixmapDirty;
};

#endif // GRAPHICSLABEL_P_H
