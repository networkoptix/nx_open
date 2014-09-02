#ifndef FISHEYE_CALIBRATION_WIDGET_H
#define FISHEYE_CALIBRATION_WIDGET_H

#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

#include <ui/widgets/framed_label.h>

namespace Ui {
class QnFisheyeCalibrationWidget;
}

class QTimer;
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

    void init();

    void updatePage();
    void updateImage();
signals:
    void dataChanged();

public slots:
    void setImageProvider(QnImageProvider *provider);
    QnImageProvider* imageProvider() const;

    void setCenter(const QPointF &center);
    void setRadius(qreal radius);
    void setHorizontalStretch(const qreal &value);
    qreal horizontalStretch() const;
private slots:
    void at_calibrator_finished(int errorCode);
    void at_autoButton_clicked();
    void at_image_animationFinished();

    void at_xCenterSlider_valueChanged(int value);
    void at_stretchSlider_valueChanged(int value);
    void at_yCenterSlider_valueChanged(int value);
    void at_radiusSlider_valueChanged(int value);

    void at_calibrator_centerChanged(const QPointF &center);
    void at_calibrator_radiusChanged(qreal radius);
    void at_calibrator_stretchChanged(qreal radius);
private:
    QScopedPointer<Ui::QnFisheyeCalibrationWidget> ui;
    QScopedPointer<QnFisheyeCalibrator> m_calibrator;
    QnImageProvider* m_imageProvider;
    QTimer* m_updateTimer;

    bool m_updating;
    int m_lastError;
    bool m_inLoading;

};

#endif // FISHEYE_CALIBRATION_WIDGET_H
