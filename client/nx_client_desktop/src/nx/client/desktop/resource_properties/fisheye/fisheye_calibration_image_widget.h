#pragma once

#include <QtCore/QPointer>
#include <QtCore/QAbstractAnimation>

#include <QtWidgets/QWidget>

#include <ui/processors/drag_process_handler.h>

class DragProcessor;
class QAbstractAnimation;

namespace nx {
namespace client {
namespace desktop {

class FisheyeAnimatedCircle: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF center READ center WRITE setCenter)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius)
    Q_PROPERTY(qreal stretch READ stretch WRITE setStretch)

public:
    explicit FisheyeAnimatedCircle(QObject *parent = 0):
        QObject(parent),
        m_center(0.5, 0.5),
        m_radius(0.5),
        m_stretch(1.0)
        {}

    QPointF center() const { return m_center; }
    Q_SLOT  void setCenter(const QPointF &center) {m_center = center; emit changed();}

    qreal   radius() const {return m_radius;}
    Q_SLOT  void setRadius(qreal radius) {m_radius = radius; emit changed();}

    qreal   stretch() const {return m_stretch;}
    Q_SLOT  void setStretch(qreal stretch) {m_stretch = stretch; emit changed();}

signals:
    void changed();
private:
    QPointF m_center;
    qreal   m_radius;
    qreal   m_stretch;
};

class FisheyeCalibrationImageWidget : public QWidget, public DragProcessHandler
{
    Q_OBJECT
    Q_PROPERTY(QImage   image       READ image      WRITE setImage)
    Q_PROPERTY(QColor   frameColor  READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QColor   lineColor   READ lineColor  WRITE setLineColor)
    Q_PROPERTY(QPointF  center      READ center     WRITE setCenter)
    Q_PROPERTY(qreal    radius      READ radius     WRITE setRadius)
    Q_PROPERTY(qreal    stretch     READ stretch    WRITE setStretch)
    Q_PROPERTY(int      lineWidth   READ lineWidth  WRITE setLineWidth)

    typedef QWidget base_type;
public:
    explicit FisheyeCalibrationImageWidget(QWidget *parent = 0);
    virtual ~FisheyeCalibrationImageWidget();

    QImage  image() const;
    Q_SLOT  void setImage(const QImage &image);

    QColor  frameColor() const;
    Q_SLOT  void setFrameColor(const QColor &color);

    QColor  lineColor() const;
    Q_SLOT  void setLineColor(const QColor &color);

    QPointF center() const;
    Q_SLOT  void setCenter(const QPointF &center);

    qreal   radius() const;
    Q_SLOT  void setRadius(qreal radius);

    qreal   stretch() const;
    Q_SLOT  void setStretch(qreal stretch);

    int     lineWidth() const;
    Q_SLOT  void setLineWidth(int width);

    void    beginSearchAnimation();
    void    endSearchAnimation();
    void    abortSearchAnimation();
signals:
    void centerModified(const QPointF &center);
    void radiusModified(qreal radius);

    void animationFinished();
protected:
    virtual void paintEvent(QPaintEvent *event) override;

    virtual void dragMove(DragInfo *info) override;

    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;

private:
    void paintCircle(QPainter* painter, const QRect &targetRect, const QPointF &relativeCenter, const qreal relativeRadius, const qreal xStretch);

    void finishAnimationStep();
    Q_SLOT void endAnimation();

    DragProcessor *m_dragProcessor;
    QImage  m_cachedImage;
    QRect   m_cachedRect;

    enum AnimationStage {
        Idle,
        Searching,
        Finishing
    };

    struct {
        AnimationStage stage;
        FisheyeAnimatedCircle *circle;
        QPointer<QAbstractAnimation> animation;
    } m_animation;

    QImage  m_image;
    QColor  m_frameColor;
    QColor  m_lineColor;
    QPointF m_center;
    qreal   m_radius;
    qreal   m_stretch;
    int     m_lineWidth;
};

} // namespace desktop
} // namespace client
} // namespace nx
