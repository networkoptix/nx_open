#ifndef FISHEYE_CALIBRATION_IMAGE_WIDGET_H
#define FISHEYE_CALIBRATION_IMAGE_WIDGET_H

#include <QWidget>

class QnFisheyeCalibrationImageWidget : public QWidget
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

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    QImage  m_image;
    QColor  m_frameColor;
    QColor  m_lineColor;
    QPointF m_center;
    qreal   m_radius;
    int     m_lineWidth;

};

#endif // FISHEYE_CALIBRATION_IMAGE_WIDGET_H
