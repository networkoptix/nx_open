#ifndef FISHEYE_CALIBRATION_WIDGET_H
#define FISHEYE_CALIBRATION_WIDGET_H

#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

#include <ui/widgets/framed_label.h>

namespace Ui {
class QnFisheyeCalibrationWidget;
}

class QnFisheyeCalibrator;
class QnImageProvider;

class QnFisheyeCalibrationWidget : public Connective<QWidget>
{
    Q_OBJECT

    typedef Connective<QWidget> base_type;
public:
    explicit QnFisheyeCalibrationWidget(QWidget *parent = 0);
    ~QnFisheyeCalibrationWidget();

    QPointF center() const;
    qreal radius() const;

public slots:
    void setImageProvider(QnImageProvider *provider);

    void setCenter(const QPointF &center);
    void setRadius(qreal radius);

private slots:
    void at_calibrator_finished(int errorCode);
    void at_autoButton_clicked();
    void at_manualButton_clicked();

    void at_xCenterSlider_valueChanged(int value);
    void at_yCenterSlider_valueChanged(int value);
    void at_radiusSlider_valueChanged(int value);
private:
    void updateManualMode();

private:
    QScopedPointer<Ui::QnFisheyeCalibrationWidget> ui;
    QScopedPointer<QnFisheyeCalibrator> m_calibrator;
    QnImageProvider* m_imageProvider;

    bool m_manualMode;

};

#endif // FISHEYE_CALIBRATION_WIDGET_H
