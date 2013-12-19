#ifndef FISHEYE_CALIBRATION_WIDGET_H
#define FISHEYE_CALIBRATION_WIDGET_H

#include <QtWidgets/QWidget>

#include <utils/image_provider.h>
#include <utils/common/connective.h>

#include <ui/widgets/framed_label.h>

namespace Ui {
class QnFisheyeCalibrationWidget;
}

class QnFisheyeCalibrator;

class QnFisheyeCalibrationWidget : public Connective<QWidget>, public QPaintDelegate
{
    Q_OBJECT

    typedef Connective<QWidget> base_type;
public:
    explicit QnFisheyeCalibrationWidget(QWidget *parent = 0);
    ~QnFisheyeCalibrationWidget();

    QPointF center() const;
    qreal radius() const;

    virtual void delegatedPaint(const QWidget* widget, QPainter *painter) override;

public slots:
    void setImageProvider(QnImageProvider *provider);

    void setCenter(const QPointF &center);
    void setRadius(qreal radius);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private slots:
    void at_imageLoaded(const QImage &image);

    void at_calibrator_finished();
    void at_startButton_clicked();

private:
    QScopedPointer<Ui::QnFisheyeCalibrationWidget> ui;

    QImage m_image;
    QScopedPointer<QnFisheyeCalibrator> m_calibrator;
};

#endif // FISHEYE_CALIBRATION_WIDGET_H
