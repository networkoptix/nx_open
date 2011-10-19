#ifndef QN_BOUNDING_INSTRUMENT_H
#define QN_BOUNDING_INSTRUMENT_H

#include "instrument.h"
#include <QRect>
#include <QTransform>

class BoundingInstrument: public Instrument {
    Q_OBJECT;
public:
    BoundingInstrument(QObject *parent = NULL);

    void setView(QGraphicsView *view);

    void setMoveRect(const QRectF &moveRect, qreal extensionMultiplier);

    void setZoomRect(const QRectF &zoomRect, qreal extensionMultiplier);

    void setMoveBorder(qreal moveBorder, qreal extensionMultiplier);

    void setZoomBorder(qreal zoomBorder, qreal extensionMultiplier);

    void setSpeed(qreal speed, qreal extensionMultiplier);

    void setZoomAnimated(bool zoomAnimated);

    void setMoveAnimated(bool moveAnimated);

protected:
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;
    virtual bool resizeEvent(QGraphicsView *view, QResizeEvent *event) override;

private:
    QGraphicsView *m_view;
    QRectF m_moveRect;
    QRectF m_zoomRect;
    qreal m_moveBorder;
    qreal m_zoomBorder;
    qreal m_moveRectExtension;
    qreal m_zoomRectExtension;
    qreal m_moveBorderExtension;
    qreal m_zoomBorderExtension;
    bool m_isZoomAnimated;
    bool m_isMoveAnimated;
    qreal m_speed;
    qreal m_speedExtension;

    QTransform m_viewToScene;
    QRectF m_viewportRect;

    QTransform m_oldViewToScene;
    QRectF m_oldViewportRect;

    bool m_cacheDirty;
};

#endif // QN_BOUNDING_INSTRUMENT_H
