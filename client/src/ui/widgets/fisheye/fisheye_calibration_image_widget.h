#ifndef FISHEYE_CALIBRATION_IMAGE_WIDGET_H
#define FISHEYE_CALIBRATION_IMAGE_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/processors/drag_process_handler.h>

class DragProcessor;

class QnFisheyeCalibrationImageWidget : public QWidget, public DragProcessHandler
{
    Q_OBJECT
    Q_PROPERTY(QImage   image       READ image      WRITE setImage)
    Q_PROPERTY(QColor   frameColor  READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QColor   lineColor   READ lineColor  WRITE setLineColor)
    Q_PROPERTY(QPointF  center      READ center     WRITE setCenter)
    Q_PROPERTY(qreal    radius      READ radius     WRITE setRadius)
    Q_PROPERTY(int      lineWidth   READ lineWidth  WRITE setLineWidth)

    typedef QWidget base_type;
public:
    explicit QnFisheyeCalibrationImageWidget(QWidget *parent = 0);
    virtual ~QnFisheyeCalibrationImageWidget();

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

    int     lineWidth() const;
    Q_SLOT  void setLineWidth(int width);

    void    beginSearchAnimation();
    void    endSearchAnimation();
signals:
    void centerModified(const QPointF &center);
    void radiusModified(qreal radius);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

    virtual void dragMove(DragInfo *info) override;

    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;

private:
    void paintCircle(QPainter* painter, const QRect &targetRect, const QPointF &relativeCenter, const qreal relativeRadius, bool paintCenter = false);

    DragProcessor *m_dragProcessor;
    QImage  m_cachedImage;
    QRect   m_cachedRect;

    enum AnimationStage {
        Idle,
        Searching,
        Radius,
        Center
    };

    struct {
        AnimationStage stage;
        QPointF center;
        qreal radius;
        QList<qreal> waves;
        qint64 timestamp;
    } m_animation;

    QImage  m_image;
    QColor  m_frameColor;
    QColor  m_lineColor;
    QPointF m_center;
    qreal   m_radius;
    int     m_lineWidth;
};

#endif // FISHEYE_CALIBRATION_IMAGE_WIDGET_H
