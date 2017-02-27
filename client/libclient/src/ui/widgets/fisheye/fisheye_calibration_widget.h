#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

#include <ui/widgets/common/framed_label.h>

namespace Ui {
class QnFisheyeCalibrationWidget;
}

class QTimer;
class QnFisheyeCalibrator;
class QnImageProvider;

class QnFisheyeCalibrationWidget : public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;

public:
    explicit QnFisheyeCalibrationWidget(QWidget* parent = nullptr);
    virtual ~QnFisheyeCalibrationWidget();

    QPointF center() const;
    void setCenter(const QPointF& center);

    qreal radius() const;
    void setRadius(qreal radius);

    void setHorizontalStretch(const qreal& value);
    qreal horizontalStretch() const;

    void setImageProvider(QnImageProvider *provider);
    QnImageProvider* imageProvider() const;

    void init();

    void autoCalibrate();

    void updatePage();
    void updateImage();

signals:
    void autoCalibrationFinished();
    void dataChanged();

private:
    void at_calibrator_finished(int errorCode);
    void at_image_animationFinished();

private:
    QScopedPointer<Ui::QnFisheyeCalibrationWidget> ui;
    QScopedPointer<QnFisheyeCalibrator> m_calibrator;
    QPointer<QnImageProvider> m_imageProvider;
    QTimer* m_updateTimer;

    int m_lastError;
    bool m_inLoading;
};
